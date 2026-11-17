// Unit tests for onnx_alignment.h: the patch that adds the alignment output to a Voice model, and the cache of the
// patched copies. The models are tiny protobufs built here by hand (a graph with a few nodes and outputs); no real
// Voice is needed. The hostile cases (truncated, bad varint, a length past the end) must be refused, not crash.
#include <doctest/doctest.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "tts/onnx_alignment.h"

namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------- building protobuf ---

void Varint(std::string* out, uint64_t v) {
    while (v >= 0x80) {
        out->push_back(static_cast<char>((v & 0x7F) | 0x80));
        v >>= 7;
    }
    out->push_back(static_cast<char>(v));
}

// A length-delimited field.
std::string Bytes(uint32_t number, const std::string& payload) {
    std::string out;
    Varint(&out, (static_cast<uint64_t>(number) << 3) | 2);
    Varint(&out, payload.size());
    return out + payload;
}

// A varint field.
std::string Num(uint32_t number, uint64_t value) {
    std::string out;
    Varint(&out, static_cast<uint64_t>(number) << 3);
    Varint(&out, value);
    return out;
}

// NodeProto: input 1, output 2, op_type 4.
std::string Node(const std::string& op, const std::string& output) {
    return Bytes(1, "x") + Bytes(2, output) + Bytes(4, op);
}

// ValueInfoProto with a name (field 1).
std::string ValueInfo(const std::string& name) { return Bytes(1, name); }

// A model: ir_version, the graph (field 7) with `nodes` and `outputs`, then opset_import (field 8) after it.
std::string Model(const std::vector<std::string>& nodes, const std::vector<std::string>& outputs) {
    std::string graph;
    for (const std::string& n : nodes) graph += Bytes(1, n);
    graph += Bytes(2, "voice");  // the graph's name, between nodes and outputs
    for (const std::string& o : outputs) graph += Bytes(12, ValueInfo(o));
    return Num(1, 8) + Bytes(7, graph) + Bytes(8, "opset");
}

// The usual Piper model: one Ceil node and one output.
std::string PiperLike() { return Model({Node("Add", "a"), Node("Ceil", "w_ceil"), Node("Mul", "b")}, {"output"}); }

struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        path = fs::temp_directory_path() / ("lexia_onnx_alignment_test_" + std::to_string(++counter) + "_" +
                                            std::to_string(reinterpret_cast<uintptr_t>(this)));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void Write(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << bytes;
}

std::string Read(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

size_t CountFiles(const fs::path& dir) {
    size_t n = 0;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) ++n;
    return n;
}

}  // namespace

// ------------------------------------------------------------ the patch ---

TEST_CASE("HasAlignmentOutput: one output is not enough, two are") {
    CHECK_FALSE(HasAlignmentOutput(PiperLike()));
    CHECK(HasAlignmentOutput(Model({Node("Ceil", "w_ceil")}, {"output", "w_ceil"})));
}

TEST_CASE("AddAlignmentOutput: the Ceil tensor becomes a second output and nothing else changes") {
    const std::string model = PiperLike();
    std::string why;
    const std::string patched = AddAlignmentOutput(model, &why);
    REQUIRE_FALSE(patched.empty());
    CHECK(why.empty());
    CHECK(HasAlignmentOutput(patched));
    CHECK(patched.size() > model.size());
    // The model is the same model with a graph that is longer: it equals the hand-built one with the output added.
    CHECK(patched == Model({Node("Add", "a"), Node("Ceil", "w_ceil"), Node("Mul", "b")}, {"output", "w_ceil"}));
    // The fields after the graph (opset_import) are kept.
    const std::string opset = Bytes(8, "opset");
    CHECK(patched.substr(patched.size() - opset.size()) == opset);
}

TEST_CASE("AddAlignmentOutput: a model that has the output already comes back unchanged") {
    const std::string model = Model({Node("Ceil", "w_ceil")}, {"output", "w_ceil"});
    std::string why;
    CHECK(AddAlignmentOutput(model, &why) == model);
    // Patching a patched model is the same as patching once.
    const std::string once = AddAlignmentOutput(PiperLike(), &why);
    CHECK(AddAlignmentOutput(once, &why) == once);
}

TEST_CASE("AddAlignmentOutput: no Ceil node, or two different Ceil tensors, is refused with a reason") {
    std::string why;
    CHECK(AddAlignmentOutput(Model({Node("Add", "a")}, {"output"}), &why).empty());
    CHECK_FALSE(why.empty());
    why.clear();
    CHECK(AddAlignmentOutput(Model({Node("Ceil", "c1"), Node("Ceil", "c2")}, {"output"}), &why).empty());
    CHECK_FALSE(why.empty());
    // Two nodes with the same output tensor are one tensor.
    CHECK_FALSE(AddAlignmentOutput(Model({Node("Ceil", "c1"), Node("Ceil", "c1")}, {"output"}), nullptr).empty());
}

TEST_CASE("AddAlignmentOutput: a model with no graph or two graphs is refused") {
    std::string why;
    CHECK(AddAlignmentOutput(Num(1, 8) + Bytes(8, "opset"), &why).empty());
    CHECK_FALSE(why.empty());
    const std::string graph = Bytes(7, Bytes(1, Node("Ceil", "c")));
    CHECK(AddAlignmentOutput(graph + graph, &why).empty());
}

TEST_CASE("AddAlignmentOutput: a truncated, empty or garbled model is refused and never crashes") {
    const std::string model = PiperLike();
    std::string why;
    CHECK(AddAlignmentOutput("", &why).empty());
    // Every cut of the model: refused, or (a cut at a field edge) a model that still has a whole graph.
    for (size_t cut = 0; cut < model.size(); ++cut) {
        const std::string part = model.substr(0, cut);
        const std::string result = AddAlignmentOutput(part, nullptr);
        if (!result.empty()) CHECK(HasAlignmentOutput(result));
        HasAlignmentOutput(part);  // must not crash either
    }
    // A graph whose length runs past the end of the file.
    std::string longGraph;
    Varint(&longGraph, (7u << 3) | 2);
    Varint(&longGraph, 1000);
    longGraph += "short";
    CHECK(AddAlignmentOutput(longGraph, &why).empty());
    CHECK_FALSE(HasAlignmentOutput(longGraph));
}

TEST_CASE("AddAlignmentOutput: a varint of more than 10 bytes, field number 0 and a group wire type are refused") {
    std::string why;
    CHECK(AddAlignmentOutput(std::string(12, '\xFF'), &why).empty());
    std::string zeroField;
    Varint(&zeroField, 2);  // field number 0, wire type 2
    zeroField += std::string("\x01x", 2);
    CHECK(AddAlignmentOutput(zeroField, &why).empty());
    std::string group;
    Varint(&group, (7u << 3) | 3);  // start group
    CHECK(AddAlignmentOutput(group, &why).empty());
    // The same inside the graph: a node whose own length runs past the node.
    const std::string badNode = Num(1, 8) + Bytes(7, Bytes(1, std::string("\x0A\x10x", 3)));
    CHECK(AddAlignmentOutput(badNode, &why).empty());
}

TEST_CASE("AddAlignmentOutput: other field types in the model (64-bit, 32-bit) are skipped") {
    std::string fixed;
    Varint(&fixed, (30u << 3) | 1);
    fixed += std::string(8, '\x01');
    Varint(&fixed, (31u << 3) | 5);
    fixed += std::string(4, '\x02');
    const std::string model = fixed + PiperLike();
    CHECK_FALSE(AddAlignmentOutput(model, nullptr).empty());
    // A 64-bit field with fewer than 8 bytes left is refused.
    std::string shortFixed;
    Varint(&shortFixed, (30u << 3) | 1);
    shortFixed += "abc";
    CHECK(AddAlignmentOutput(shortFixed, nullptr).empty());
}

// ------------------------------------------------------------- the cache ---

TEST_CASE("EnsureAlignedCopy: makes the copy with its config, and uses it the second time") {
    TempDir voices, aligned;
    Write(voices.path / "en_US-amy-medium.onnx", PiperLike());
    Write(voices.path / "en_US-amy-medium.onnx.json", "{\"a\":1}");
    const AlignedCopy first = EnsureAlignedCopy(voices.path / "en_US-amy-medium.onnx", aligned.path / "aligned");
    REQUIRE_FALSE(first.model.empty());
    CHECK(first.created);
    CHECK(first.why.empty());
    CHECK(HasAlignmentOutput(Read(first.model)));
    CHECK(Read(fs::path(first.model.wstring() + L".json")) == "{\"a\":1}");
    CHECK(first.model.filename().string().rfind("en_US-amy-medium.", 0) == 0);
    CHECK(CountFiles(aligned.path / "aligned") == 2);  // the model and the config, no .part file

    const AlignedCopy second = EnsureAlignedCopy(voices.path / "en_US-amy-medium.onnx", aligned.path / "aligned");
    CHECK(second.model == first.model);
    CHECK_FALSE(second.created);
}

TEST_CASE("EnsureAlignedCopy: a changed Voice file gets a new copy and the old one goes") {
    TempDir voices, aligned;
    const fs::path model = voices.path / "v.onnx";
    Write(model, PiperLike());
    Write(voices.path / "v.onnx.json", "{}");
    const AlignedCopy first = EnsureAlignedCopy(model, aligned.path);
    REQUIRE_FALSE(first.model.empty());
    // Another size (and time): another key.
    Write(model, Model({Node("Ceil", "w_ceil"), Node("Add", "padding padding")}, {"output"}));
    const AlignedCopy second = EnsureAlignedCopy(model, aligned.path);
    REQUIRE_FALSE(second.model.empty());
    CHECK(second.created);
    CHECK(second.model != first.model);
    CHECK_FALSE(fs::exists(first.model));
    CHECK(CountFiles(aligned.path) == 2);
}

TEST_CASE("EnsureAlignedCopy: the copies of another Voice are left alone") {
    TempDir voices, aligned;
    Write(aligned.path / "other.1-2.onnx", "x");
    Write(aligned.path / "v-high.1-2.onnx", "x");  // a longer name that starts with "v"
    Write(voices.path / "v.onnx", PiperLike());
    Write(voices.path / "v.onnx.json", "{}");
    Write(voices.path / "w.onnx", PiperLike());
    Write(voices.path / "w.onnx.json", "{}");
    REQUIRE_FALSE(EnsureAlignedCopy(voices.path / "v.onnx", aligned.path).model.empty());
    REQUIRE_FALSE(EnsureAlignedCopy(voices.path / "w.onnx", aligned.path).model.empty());
    CHECK(fs::exists(aligned.path / "other.1-2.onnx"));
    CHECK(fs::exists(aligned.path / "v-high.1-2.onnx"));
    CHECK(CountFiles(aligned.path) == 6);
}

TEST_CASE("EnsureAlignedCopy: a model that has the output is loaded as it is, and nothing is written") {
    TempDir voices, aligned;
    Write(voices.path / "v.onnx", Model({Node("Ceil", "w_ceil")}, {"output", "w_ceil"}));
    Write(voices.path / "v.onnx.json", "{}");
    const AlignedCopy r = EnsureAlignedCopy(voices.path / "v.onnx", aligned.path / "aligned");
    CHECK(r.model.empty());
    CHECK(r.alreadyAligned);
    CHECK_FALSE(fs::exists(aligned.path / "aligned"));
}

TEST_CASE("EnsureAlignedCopy: it refuses with a reason and writes nothing") {
    TempDir voices, aligned;
    Write(voices.path / "v.onnx", PiperLike());
    Write(voices.path / "v.onnx.json", "{}");
    // No folder for the copies.
    AlignedCopy r = EnsureAlignedCopy(voices.path / "v.onnx", fs::path());
    CHECK(r.model.empty());
    CHECK_FALSE(r.why.empty());
    // A model that is not there, and one over the size limit.
    CHECK(EnsureAlignedCopy(voices.path / "nothing.onnx", aligned.path).model.empty());
    r = EnsureAlignedCopy(voices.path / "v.onnx", aligned.path, /*maxBytes=*/10);
    CHECK(r.model.empty());
    CHECK_FALSE(r.why.empty());
    // A model that is not ONNX, a model with no config.
    Write(voices.path / "junk.onnx", "this is not a model");
    Write(voices.path / "junk.onnx.json", "{}");
    CHECK(EnsureAlignedCopy(voices.path / "junk.onnx", aligned.path).model.empty());
    Write(voices.path / "noconfig.onnx", PiperLike());
    CHECK(EnsureAlignedCopy(voices.path / "noconfig.onnx", aligned.path).model.empty());
    // The folder for the copies is a file.
    Write(aligned.path / "afile", "x");
    CHECK(EnsureAlignedCopy(voices.path / "v.onnx", aligned.path / "afile").model.empty());
    // Nothing was left behind in the folder (only "afile").
    CHECK(CountFiles(aligned.path) == 1);
}

TEST_CASE("EnsureAlignedCopy: a folder that is a junction is not written through") {
    TempDir voices, aligned, elsewhere;
    Write(voices.path / "v.onnx", PiperLike());
    Write(voices.path / "v.onnx.json", "{}");
    const fs::path link = aligned.path / "link";
    // mklink /J needs no administrator right; skip quietly where it cannot be made.
    const std::string cmd = "mklink /J \"" + link.string() + "\" \"" + elsewhere.path.string() + "\" >nul 2>nul";
    if (std::system(cmd.c_str()) != 0 || !fs::exists(link)) return;
    const AlignedCopy r = EnsureAlignedCopy(voices.path / "v.onnx", link);
    CHECK(r.model.empty());
    CHECK_FALSE(r.why.empty());
    CHECK(CountFiles(elsewhere.path) == 0);
    Write(elsewhere.path / "v.1-2.onnx", "x");
    RemoveAlignedCopies(link, "v");  // must not delete through the junction
    CHECK(fs::exists(elsewhere.path / "v.1-2.onnx"));
    fs::remove(link);  // the junction only
}

TEST_CASE("RemoveAlignedCopies: every key of the Voice and its leftovers, nothing else") {
    TempDir aligned;
    Write(aligned.path / "v.1-2.onnx", "x");
    Write(aligned.path / "v.1-2.onnx.json", "x");
    Write(aligned.path / "v.3-4.onnx.part", "x");
    Write(aligned.path / "v.3-4.onnx.json.part", "x");
    Write(aligned.path / "v.notakey.onnx", "x");
    Write(aligned.path / "v2.1-2.onnx", "x");
    Write(aligned.path / "note.txt", "x");
    RemoveAlignedCopies(aligned.path, "v");
    CHECK(CountFiles(aligned.path) == 3);  // v.notakey.onnx, v2.1-2.onnx, note.txt
    RemoveAlignedCopies(aligned.path, "");  // no name: nothing
    RemoveAlignedCopies(aligned.path / "missing", "v");
    CHECK(CountFiles(aligned.path) == 3);
}

TEST_CASE("AlignedDirIn: the aligned folder of the data folder; none without one") {
    CHECK(AlignedDirIn(fs::path("C:\\data\\Lexia")) == fs::path("C:\\data\\Lexia") / L"aligned");
    CHECK(AlignedDirIn(fs::path()).empty());
}
