#include "tts/onnx_alignment.h"

#include <cctype>
#include <fstream>
#include <set>
#include <system_error>
#include <vector>

#include "common.h"

namespace fs = std::filesystem;

namespace {

// ------------------------------------------------------------- reading protobuf ---
// An ONNX file is a protobuf: ModelProto (the graph is field 7), GraphProto (nodes are field 1, outputs are
// field 12), NodeProto (outputs are field 2, the operator is field 4), ValueInfoProto (the name is field 1).
// The reader below is bounds checked everywhere: a field that does not fit inside its parent ends the read.

constexpr uint32_t kModelGraph = 7;
constexpr uint32_t kGraphNode = 1;
constexpr uint32_t kGraphOutput = 12;
constexpr uint32_t kNodeOutput = 2;
constexpr uint32_t kNodeOpType = 4;
constexpr uint32_t kValueInfoName = 1;

// Reads a varint at b[*pos], not past `limit`. At most 10 bytes (a 64-bit value). False on a short or too
// long one; *pos is then not meaningful.
bool ReadVarint(const std::string& b, size_t* pos, size_t limit, uint64_t* value) {
    *value = 0;
    for (int shift = 0; shift < 70; shift += 7) {
        if (*pos >= limit) return false;
        const unsigned char c = static_cast<unsigned char>(b[(*pos)++]);
        if (shift < 64) *value |= static_cast<uint64_t>(c & 0x7F) << shift;
        if (!(c & 0x80)) return true;
    }
    return false;  // more than 10 bytes
}

// One field of a message that occupies [begin, limit): its number, its wire type, where its tag starts, and,
// for a length-delimited field (wire type 2), where its bytes start and end. `next` is where the next field
// starts.
struct WireField {
    uint32_t number = 0;
    int wire = 0;
    size_t tagStart = 0;
    size_t dataStart = 0;
    size_t dataEnd = 0;
    size_t next = 0;
};

// Reads the field at `pos`. False at the end of the message (pos == limit), and on anything malformed: a bad
// varint, field number 0, a wire type that an ONNX file does not use (groups, 6, 7) or a field that runs past
// `limit`.
bool NextField(const std::string& b, size_t pos, size_t limit, WireField* f) {
    if (pos >= limit || limit > b.size()) return false;
    f->tagStart = pos;
    uint64_t tag = 0;
    if (!ReadVarint(b, &pos, limit, &tag)) return false;
    if ((tag >> 3) == 0 || (tag >> 3) > 0x1FFFFFFF) return false;
    f->number = static_cast<uint32_t>(tag >> 3);
    f->wire = static_cast<int>(tag & 7);
    switch (f->wire) {
    case 0: {  // varint
        uint64_t ignored = 0;
        if (!ReadVarint(b, &pos, limit, &ignored)) return false;
        f->dataStart = f->dataEnd = pos;
        break;
    }
    case 1:  // 64-bit
        if (limit - pos < 8) return false;
        f->dataStart = pos;
        pos += 8;
        f->dataEnd = pos;
        break;
    case 5:  // 32-bit
        if (limit - pos < 4) return false;
        f->dataStart = pos;
        pos += 4;
        f->dataEnd = pos;
        break;
    case 2: {  // length-delimited
        uint64_t len = 0;
        if (!ReadVarint(b, &pos, limit, &len) || len > limit - pos) return false;
        f->dataStart = pos;
        f->dataEnd = pos + static_cast<size_t>(len);
        pos = f->dataEnd;
        break;
    }
    default:
        return false;
    }
    f->next = pos;
    return true;
}

void AppendVarint(std::string* out, uint64_t value) {
    while (value >= 0x80) {
        out->push_back(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out->push_back(static_cast<char>(value));
}

// What the graph of a model holds that matters here.
struct GraphInfo {
    WireField graph;                 // the graph field of the model
    std::set<std::string> ceil;      // the output tensor names of the Ceil nodes
    size_t outputCount = 0;          // how many graph outputs there are
    std::set<std::string> outputs;   // their names
};

// Walks the model once: finds the graph, and in it the Ceil tensors and the outputs. False and `why` for
// anything that is not a well-formed model with one graph. Three levels deep and no more, so a hostile file
// cannot run the stack out; the work is linear in the file size.
bool Analyse(const std::string& model, GraphInfo* info, std::string* why) {
    bool found = false;
    for (size_t pos = 0; pos < model.size();) {
        WireField f;
        if (!NextField(model, pos, model.size(), &f)) {
            *why = "the file is not a well-formed ONNX model";
            return false;
        }
        pos = f.next;
        if (f.number != kModelGraph || f.wire != 2) continue;
        if (found) {
            *why = "the model has more than one graph";
            return false;
        }
        info->graph = f;
        found = true;
    }
    if (!found) {
        *why = "the model has no graph";
        return false;
    }
    const WireField& graph = info->graph;
    for (size_t pos = graph.dataStart; pos < graph.dataEnd;) {
        WireField f;
        if (!NextField(model, pos, graph.dataEnd, &f)) {
            *why = "the graph is not well formed";
            return false;
        }
        pos = f.next;
        if (f.wire != 2) continue;
        if (f.number == kGraphNode) {
            std::string opType;
            std::vector<std::string> nodeOutputs;
            for (size_t p = f.dataStart; p < f.dataEnd;) {
                WireField g;
                if (!NextField(model, p, f.dataEnd, &g)) {
                    *why = "a node is not well formed";
                    return false;
                }
                p = g.next;
                if (g.wire != 2) continue;
                if (g.number == kNodeOpType) opType.assign(model, g.dataStart, g.dataEnd - g.dataStart);
                if (g.number == kNodeOutput) nodeOutputs.emplace_back(model, g.dataStart, g.dataEnd - g.dataStart);
            }
            if (opType == "Ceil") info->ceil.insert(nodeOutputs.begin(), nodeOutputs.end());
        } else if (f.number == kGraphOutput) {
            ++info->outputCount;
            for (size_t p = f.dataStart; p < f.dataEnd;) {
                WireField g;
                if (!NextField(model, p, f.dataEnd, &g)) {
                    *why = "a graph output is not well formed";
                    return false;
                }
                p = g.next;
                if (g.number == kValueInfoName && g.wire == 2) {
                    info->outputs.emplace(model, g.dataStart, g.dataEnd - g.dataStart);
                }
            }
        }
    }
    return true;
}

// ------------------------------------------------------------------- the cache ---

// Is this a link (a symbolic link or a directory junction)? Lexia's own files are never links, so a link in the
// aligned folder is somebody else's doing: nothing is written through it.
bool IsLink(const fs::path& p) {
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(p, ec);
    return !ec && (st.type() == fs::file_type::symlink || st.type() == fs::file_type::junction);
}

// The file name of a copy of `name`, without its extension: name.<size>-<time>.
std::string CopyBase(const std::string& name, std::uintmax_t size, fs::file_time_type time) {
    return name + "." + std::to_string(size) + "-" +
           std::to_string(static_cast<unsigned long long>(time.time_since_epoch().count()));
}

// Is `file` one of the files of an aligned copy of the Voice `name` (any key)? That is name + "." + digits + "-"
// + digits + one of the four endings. A longer Voice name that starts with this one does not match, as the
// part after "name." must be the key.
bool IsCopyOf(const std::string& file, const std::string& name) {
    if (file.size() <= name.size() + 1 || file.compare(0, name.size(), name) != 0 || file[name.size()] != '.') return false;
    size_t i = name.size() + 1;
    auto digits = [&]() {
        const size_t start = i;
        while (i < file.size() && std::isdigit(static_cast<unsigned char>(file[i]))) ++i;
        return i > start;
    };
    if (!digits() || i >= file.size() || file[i] != '-') return false;
    ++i;
    if (!digits()) return false;
    const std::string ending = file.substr(i);
    return ending == ".onnx" || ending == ".onnx.json" || ending == ".onnx.part" || ending == ".onnx.json.part";
}

// Deletes the files of every copy of `name` in `dir` except `keep` (a file name; "" keeps none). Best effort.
void RemoveCopies(const fs::path& dir, const std::string& name, const std::string& keepBase) {
    std::error_code ec;
    if (IsLink(dir) || !fs::is_directory(dir, ec)) return;
    std::vector<fs::path> doomed;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        const fs::file_status st = it->symlink_status(ec);
        if (ec || st.type() != fs::file_type::regular) continue;  // regular files only: never a link
        const std::string file = it->path().filename().u8string();
        if (!IsCopyOf(file, name)) continue;
        if (!keepBase.empty() && file.compare(0, keepBase.size(), keepBase) == 0 &&
            (file.size() == keepBase.size() || file[keepBase.size()] == '.')) {
            continue;  // a file of the copy that stays
        }
        doomed.push_back(it->path());
    }
    for (const fs::path& p : doomed) fs::remove(p, ec);
}

// Writes `bytes` to `part`, then renames it to `final`. A failure leaves no `.part` file behind.
bool WriteThenRename(const std::string& bytes, const fs::path& part, const fs::path& final, std::string* why) {
    std::error_code ec;
    if (IsLink(part) || IsLink(final)) {
        *why = "a file of the aligned folder is a link";
        return false;
    }
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out) {
            fs::remove(part, ec);
            *why = "the copy could not be written";
            return false;
        }
    }
    fs::rename(part, final, ec);
    if (ec) {
        fs::remove(part, ec);
        *why = "the copy could not be put in place";
        return false;
    }
    return true;
}

}  // namespace

// ------------------------------------------------------------------ the patch ---

bool HasAlignmentOutput(const std::string& onnxBytes) {
    GraphInfo info;
    std::string why;
    return Analyse(onnxBytes, &info, &why) && info.outputCount > 1;
}

std::string AddAlignmentOutput(const std::string& onnxBytes, std::string* why) {
    std::string reason;
    GraphInfo info;
    if (!Analyse(onnxBytes, &info, &reason)) {
        if (why) *why = reason;
        return std::string();
    }
    if (info.outputCount > 1) return onnxBytes;  // already has the output (a patched Voice)
    if (info.ceil.size() != 1) {
        if (why) *why = "the graph has " + std::to_string(info.ceil.size()) + " Ceil tensors, not one";
        return std::string();
    }
    const std::string& name = *info.ceil.begin();
    if (info.outputs.count(name)) {
        if (why) *why = "the Ceil tensor is already the output";
        return std::string();
    }
    // A ValueInfoProto with the name only (as piper's Python patch writes it), appended to the graph as `output`.
    std::string valueInfo;
    valueInfo.push_back(static_cast<char>((kValueInfoName << 3) | 2));
    AppendVarint(&valueInfo, name.size());
    valueInfo += name;
    std::string added;
    added.push_back(static_cast<char>((kGraphOutput << 3) | 2));
    AppendVarint(&added, valueInfo.size());
    added += valueInfo;

    // The model with the graph field made longer: the bytes before it, its tag, the new length, the graph's own
    // bytes and the new output, then everything after it. The graph's fields may come in any order, so appending
    // at its end is valid.
    const WireField& graph = info.graph;
    const size_t graphLength = graph.dataEnd - graph.dataStart;
    std::string out;
    out.reserve(onnxBytes.size() + added.size() + 8);
    out.append(onnxBytes, 0, graph.tagStart);
    out.push_back(static_cast<char>((kModelGraph << 3) | 2));
    AppendVarint(&out, graphLength + added.size());
    out.append(onnxBytes, graph.dataStart, graphLength);
    out += added;
    out.append(onnxBytes, graph.dataEnd, std::string::npos);
    return out;
}

// ------------------------------------------------------------------ the cache ---

fs::path AlignedDirIn(const fs::path& dataDir) { return dataDir.empty() ? dataDir : dataDir / L"aligned"; }

AlignedCopy EnsureAlignedCopy(const fs::path& model, const fs::path& alignedDir, std::uintmax_t maxBytes) {
    AlignedCopy result;
    auto refuse = [&result](const std::string& why) {
        result.model.clear();
        result.why = why;
        return result;
    };
    if (alignedDir.empty()) return refuse("there is no folder for the copies");
    std::error_code ec;
    if (!fs::is_regular_file(model, ec)) return refuse("the model is not a file");
    const std::uintmax_t size = fs::file_size(model, ec);
    if (ec) return refuse("the model's size is unknown");
    if (size > maxBytes) return refuse("the model is too big to be patched");
    const fs::file_time_type time = fs::last_write_time(model, ec);
    if (ec) return refuse("the model's time is unknown");

    // The aligned folder must be a real folder (or not there yet).
    if (IsLink(alignedDir)) return refuse("the aligned folder is a link");
    if (fs::exists(alignedDir, ec) && !fs::is_directory(alignedDir, ec)) return refuse("the aligned folder is a file");

    const std::string name = model.stem().u8string();
    const std::string base = CopyBase(name, size, time);
    const fs::path copyModel = alignedDir / fs::u8path(base + ".onnx");
    const fs::path copyConfig = alignedDir / fs::u8path(base + ".onnx.json");

    // A copy of this very file (same size and time) that is complete: use it.
    auto regular = [](const fs::path& p) {
        std::error_code e;
        return fs::symlink_status(p, e).type() == fs::file_type::regular;
    };
    if (regular(copyModel) && regular(copyConfig)) {
        std::error_code e;
        if (fs::file_size(copyModel, e) > 0 && !e) {
            result.model = copyModel;
            return result;
        }
    }

    std::string bytes;
    if (!ReadFileToString(model, &bytes, maxBytes)) return refuse("the model could not be read");
    if (HasAlignmentOutput(bytes)) {
        result.alreadyAligned = true;
        return result;  // load the original
    }
    std::string why;
    const std::string patched = AddAlignmentOutput(bytes, &why);
    bytes.clear();
    bytes.shrink_to_fit();
    if (patched.empty()) return refuse(why.empty() ? "the model could not be patched" : why);

    fs::create_directories(alignedDir, ec);
    if (ec || IsLink(alignedDir)) return refuse("the aligned folder could not be made");

    // The config first (libpiper reads "<model>.json"), then the model: a model file means a complete copy.
    std::string config;
    if (!ReadFileToString(fs::path(model.wstring() + L".json"), &config, 16ull * 1024 * 1024)) {
        return refuse("the config could not be read");
    }
    const fs::path configPart = fs::path(copyConfig.wstring() + L".part");
    const fs::path modelPart = fs::path(copyModel.wstring() + L".part");
    if (!WriteThenRename(config, configPart, copyConfig, &why) || !WriteThenRename(patched, modelPart, copyModel, &why)) {
        return refuse(why);
    }
    RemoveCopies(alignedDir, name, base);  // the copies of earlier keys of this Voice are out of date
    result.model = copyModel;
    result.created = true;
    return result;
}

void RemoveAlignedCopies(const fs::path& alignedDir, std::string_view voiceName) {
    if (alignedDir.empty() || voiceName.empty()) return;
    RemoveCopies(alignedDir, std::string(voiceName), std::string());
}
