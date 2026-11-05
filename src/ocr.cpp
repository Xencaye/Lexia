#include "ocr.h"

#include <onnxruntime_cxx_api.h>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>
#include <thread>

#include "ctc.h"
#include "log.h"
#include "lookalike.h"

// The names of a session's first input and first output.
struct IoNames {
    std::string in, out;
};

// Reads the names from the model, so the code does not depend on how the model's export named them.
static IoNames SessionIoNames(Ort::Session& s) {
    Ort::AllocatorWithDefaultOptions alloc;
    return {s.GetInputNameAllocated(0, alloc).get(), s.GetOutputNameAllocated(0, alloc).get()};
}

// Loads one ONNX model with `threads` threads for its work and every graph optimisation on. Throws
// Ort::Exception on a missing or bad model (Init catches it).
static std::unique_ptr<Ort::Session> MakeSession(Ort::Env& env, const std::filesystem::path& model, int threads) {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(threads);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    return std::make_unique<Ort::Session>(env, model.c_str(), opts);
}

// Runs a session on one float tensor (`input` laid out as `shape`) and returns its outputs.
static std::vector<Ort::Value> RunOne(Ort::Session& s, const IoNames& names, std::vector<float>& input,
                                      const std::array<int64_t, 4>& shape) {
    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(mem, input.data(), input.size(), shape.data(), shape.size());
    const char* inN[] = {names.in.c_str()};
    const char* outN[] = {names.out.c_str()};
    return s.Run(Ort::RunOptions{nullptr}, inN, &tensor, 1, outN, 1);
}

// The recogniser's dictionary: one entry per line, line n is class n + 1 (class 0 is the CTC blank).
// Windows line ends, a UTF-8 BOM and blank lines at the end are allowed. False if empty or unreadable.
static bool LoadDict(const std::filesystem::path& p, std::vector<std::string>& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    out.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
    }
    if (!out.empty() && out[0].rfind("\xEF\xBB\xBF", 0) == 0) out[0].erase(0, 3);  // BOM
    while (!out.empty() && out.back().empty()) out.pop_back();
    return !out.empty();
}

// Where the models are: next to lexia.exe if there is a models folder there (the packaged
// version). A Debug build falls back to the project's models folder (MODELS_DIR); Release
// never does, so a shipped exe holds no path from the build machine. OcrEngine::SetModelsDir
// overrides all of this (lexia_cli uses it for the dev folder; the app never calls it).
static std::filesystem::path g_modelsDirOverride;

static std::filesystem::path ResolveModelsDir() {
    if (!g_modelsDirOverride.empty()) return g_modelsDirOverride;
    const std::filesystem::path beside = ExeDir() / "models";
#ifdef MODELS_DIR
    std::error_code ec;
    if (!std::filesystem::exists(beside / "det.onnx", ec)) return std::filesystem::u8path(MODELS_DIR);
#endif
    return beside;
}

// One ONNX Runtime environment for the whole process, as ORT asks: a Language switch keeps two
// engines alive for a moment. Each engine holds a reference, so it outlives their sessions.
static std::shared_ptr<Ort::Env> SharedOrtEnv() {
    static const std::shared_ptr<Ort::Env> env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "lexia");
    return env;
}

struct OcrEngine::Impl {
    static bool ModelsPresent(const Language& lang) {
        namespace fs = std::filesystem;
        const fs::path dir = ResolveModelsDir();
        std::error_code ec;  // a file that cannot be checked counts as missing
        return fs::exists(dir / "det.onnx", ec) && fs::exists(dir / lang.recModel, ec) &&
               fs::exists(dir / lang.dictFile, ec);
    }

    // Loads the detector (once) and the recogniser for `lang`. On failure the
    // previously loaded language keeps working.
    bool Init(const Language& lang) {
        namespace fs = std::filesystem;
        const fs::path dir = ResolveModelsDir();
        const unsigned hw = std::max(2u, std::thread::hardware_concurrency());
        std::error_code ec;

        try {
            if (!det_) {
                const fs::path p = dir / "det.onnx";
                if (!fs::exists(p, ec)) {
                    LogError("Missing model file: ", p);
                    return false;
                }
                auto s = MakeSession(*env_, p, static_cast<int>(hw));
                detNames_ = SessionIoNames(*s);
                det_ = std::move(s);
            }

            const fs::path recPath = dir / lang.recModel;
            const fs::path dictPath = dir / lang.dictFile;
            if (!fs::exists(recPath, ec) || !fs::exists(dictPath, ec)) {
                LogError("Missing model files for '", lang.id, "': ", recPath, " and/or ", dictPath);
                return false;
            }

            std::vector<std::string> dict;
            if (!LoadDict(dictPath, dict)) {
                LogError("Could not read dictionary: ", dictPath);
                return false;
            }

            auto s = MakeSession(*env_, recPath, 2);
            const IoNames names = SessionIoNames(*s);

            // Check that the dictionary matches the model (blank + dict [+ space]).
            const int classes = ProbeClasses(*s, names);
            bool useSpace;
            if (classes == static_cast<int>(dict.size()) + 2) {
                useSpace = true;
            } else if (classes == static_cast<int>(dict.size()) + 1) {
                useSpace = false;
            } else {
                LogError("Dictionary/model mismatch for '", lang.id, "': the dictionary has ",
                         dict.size(), " entries but the model outputs ", classes,
                         " classes. The wrong dict file was probably downloaded.");
                return false;
            }

            rec_ = std::move(s);
            recNames_ = names;
            alphabet_ = BuildAlphabetMask(dict, useSpace, lang);  // once per engine: it is per Language
            dict_ = std::move(dict);
            useSpace_ = useSpace;
            script_ = lang.script;
            return true;
        } catch (const Ort::Exception& e) {
            LogError("ONNX Runtime error while loading models: ", e.what());
            return false;
        } catch (const std::exception& e) {  // bad_alloc, a filesystem error, ...
            LogError("Error while loading models: ", e.what());
            return false;
        }
    }

    // Finds text inside `area` of the image. Returned boxes are in full-image coordinates.
    // `isDrag`: the user dragged this area, so if nothing is detected we still try
    // to read the whole area as one piece of text (handy for a single short word).
    std::vector<TextLine> ReadLines(const cv::Mat& bgr, Rect area, bool isDrag) {
        std::vector<TextLine> lines;
        if (!det_ || !rec_) return lines;

        cv::Rect roi(area.left, area.top, area.right - area.left, area.bottom - area.top);
        roi &= cv::Rect(0, 0, bgr.cols, bgr.rows);
        if (roi.width < 8 || roi.height < 8) return lines;
        const cv::Mat region = bgr(roi);

        std::vector<cv::Rect> boxes;
        try {
            boxes = Detect(region);
        } catch (const Ort::Exception& e) {
            LogError("Detection error: ", e.what());
            return lines;
        }
        if (boxes.empty() && isDrag) boxes.push_back(cv::Rect(0, 0, region.cols, region.rows));

        // Read the pieces in parallel; ONNX Runtime sessions are safe to call from many threads.
        std::vector<RecResult> results(boxes.size());
        std::atomic<size_t> next{0};
        auto work = [&]() {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= boxes.size()) break;
                try {
                    results[i] = Recognize(region(boxes[i]));
                } catch (const std::exception&) {
                    results[i] = RecResult{};
                }
            }
        };
        const unsigned nThreads =
            std::min<unsigned>(4, std::max(1u, std::thread::hardware_concurrency()));
        std::vector<std::thread> pool;
        for (unsigned t = 1; t < nThreads; ++t) pool.emplace_back(work);
        work();
        for (std::thread& t : pool) t.join();

        for (size_t i = 0; i < boxes.size(); ++i) {
            const RecResult& r = results[i];
            // The Lookalike fix, once per Line: the one place recognised text leaves the recogniser.
            std::string text = FixLookalikes(Trim(r.text), script_);
            if (r.conf < 0.5f || text.empty() || !HasLetterOrDigit(text)) continue;

            TextLine ln;
            ln.box.left = roi.x + boxes[i].x;
            ln.box.top = roi.y + boxes[i].y;
            ln.box.right = roi.x + boxes[i].x + boxes[i].width;
            ln.box.bottom = roi.y + boxes[i].y + boxes[i].height;
            ln.text = std::move(text);
            ln.conf = r.conf * 100.f;
            lines.push_back(std::move(ln));
        }
        return lines;
    }

private:
    struct RecResult {
        std::string text;
        float conf = 0.f;  // 0..1
    };

    static int ProbeClasses(Ort::Session& s, const IoNames& names) {
        std::vector<float> input(size_t(3) * 48 * 160, 0.f);
        auto outs = RunOne(s, names, input, {1, 3, 48, 160});
        const std::vector<int64_t> sh = outs[0].GetTensorTypeAndShapeInfo().GetShape();
        return sh.size() >= 3 ? static_cast<int>(sh[2]) : -1;
    }

    // Finds text regions with the DB detector. Returns boxes in `bgr` pixel coordinates.
    std::vector<cv::Rect> Detect(const cv::Mat& bgr) {
        const int w = bgr.cols, h = bgr.rows;
        const int longSide = std::max(w, h);
        double s = 1.0;
        if (longSide > 2048) s = 2048.0 / longSide;                       // keep it fast
        else if (longSide < 960) s = std::min(3.0, 960.0 / longSide);    // enlarge small areas

        const int nw = std::max(32, static_cast<int>(std::lround(w * s / 32.0)) * 32);
        const int nh = std::max(32, static_cast<int>(std::lround(h * s / 32.0)) * 32);

        cv::Mat resized;
        cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0,
                   (nw * nh < w * h) ? cv::INTER_AREA : cv::INTER_CUBIC);

        // PaddleOCR normalises the BGR image with these values, in this channel order.
        static const float mean[3] = {0.485f, 0.456f, 0.406f};
        static const float stdv[3] = {0.229f, 0.224f, 0.225f};
        std::vector<float> input(size_t(3) * nh * nw);
        for (int y = 0; y < nh; ++y) {
            const uint8_t* row = resized.ptr<uint8_t>(y);
            for (int x = 0; x < nw; ++x) {
                for (int c = 0; c < 3; ++c) {
                    input[(size_t(c) * nh + y) * nw + x] =
                        (row[x * 3 + c] / 255.f - mean[c]) / stdv[c];
                }
            }
        }

        auto outs = RunOne(*det_, detNames_, input, {1, 3, nh, nw});

        const std::vector<int64_t> sh = outs[0].GetTensorTypeAndShapeInfo().GetShape();
        if (sh.size() < 2) return {};
        const int oh = static_cast<int>(sh[sh.size() - 2]);
        const int ow = static_cast<int>(sh[sh.size() - 1]);
        cv::Mat prob(oh, ow, CV_32FC1, outs[0].GetTensorMutableData<float>());

        cv::Mat bin;
        cv::compare(prob, 0.3, bin, cv::CMP_GT);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(bin, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        const double sx = static_cast<double>(ow) / w;
        const double sy = static_cast<double>(oh) / h;

        std::vector<cv::Rect> boxes;
        for (const auto& contour : contours) {
            if (boxes.size() >= 1000) break;
            const cv::Rect r = cv::boundingRect(contour);
            if (std::min(r.width, r.height) < 3) continue;

            // average detector confidence inside the shape
            cv::Mat mask = cv::Mat::zeros(r.size(), CV_8UC1);
            const std::vector<std::vector<cv::Point>> one = {contour};
            cv::drawContours(mask, one, 0, cv::Scalar(255), cv::FILLED, cv::LINE_8,
                             cv::noArray(), INT_MAX, -r.tl());
            if (cv::mean(prob(r), mask)[0] < 0.5) continue;

            // The detector marks a shrunken core of each text region; grow it back.
            const double area = double(r.width) * r.height;
            const double perimeter = 2.0 * (r.width + r.height);
            const double d = area * 1.6 / perimeter;

            const int x0 = static_cast<int>(std::floor((r.x - d) / sx));
            const int y0 = static_cast<int>(std::floor((r.y - d) / sy));
            const int x1 = static_cast<int>(std::ceil((r.x + r.width + d) / sx));
            const int y1 = static_cast<int>(std::ceil((r.y + r.height + d) / sy));
            cv::Rect box(x0, y0, x1 - x0, y1 - y0);
            box &= cv::Rect(0, 0, w, h);
            if (box.width < 4 || box.height < 4) continue;
            boxes.push_back(box);
        }
        return boxes;
    }

    // Reads one cropped piece of text.
    RecResult Recognize(const cv::Mat& crop) {
        RecResult res;
        if (crop.cols < 4 || crop.rows < 4) return res;

        const int H = 48;
        int W = static_cast<int>(std::lround(double(H) * crop.cols / crop.rows));
        W = std::min(4096, std::max(16, (W + 3) / 4 * 4));

        cv::Mat r;
        cv::resize(crop, r, cv::Size(W, H), 0, 0, H > crop.rows ? cv::INTER_CUBIC : cv::INTER_AREA);

        std::vector<float> input(size_t(3) * H * W);
        for (int y = 0; y < H; ++y) {
            const uint8_t* row = r.ptr<uint8_t>(y);
            for (int x = 0; x < W; ++x) {
                for (int c = 0; c < 3; ++c) {
                    input[(size_t(c) * H + y) * W + x] = row[x * 3 + c] / 127.5f - 1.f;
                }
            }
        }

        auto outs = RunOne(*rec_, recNames_, input, {1, 3, H, W});

        const std::vector<int64_t> sh = outs[0].GetTensorTypeAndShapeInfo().GetShape();
        if (sh.size() < 3) return res;
        const int T = static_cast<int>(sh[1]);
        const int C = static_cast<int>(sh[2]);
        const float* data = outs[0].GetTensorData<float>();

        // CTC greedy decoding that never picks a character outside the Language's Alphabet.
        const CtcResult decoded = CtcGreedyDecode(data, T, C, dict_, useSpace_, alphabet_);
        res.text = decoded.text;
        res.conf = decoded.conf;
        return res;
    }

    std::shared_ptr<Ort::Env> env_ = SharedOrtEnv();  // before the sessions: destroyed after them
    std::unique_ptr<Ort::Session> det_, rec_;
    IoNames detNames_, recNames_;
    std::vector<std::string> dict_;
    std::vector<char> alphabet_;  // per recogniser class: allowed by the Language's Alphabet?
    bool useSpace_ = true;
    Script script_ = Script::Latin;  // the Language's script: the Lookalike fix's tie-break
};

// ------------------------------------------------------------ public wrapper ---

OcrEngine::OcrEngine() : impl_(std::make_unique<Impl>()) {}
OcrEngine::~OcrEngine() = default;

std::string OcrEngine::ModelsDir() { return ResolveModelsDir().u8string(); }

void OcrEngine::SetModelsDir(const std::string& utf8Dir) {
    g_modelsDirOverride = std::filesystem::u8path(utf8Dir).make_preferred();
}

bool OcrEngine::ModelsPresent(const Language& lang) { return Impl::ModelsPresent(lang); }

bool OcrEngine::Init(const Language& lang) { return impl_->Init(lang); }

std::vector<TextLine> OcrEngine::ReadLines(const cv::Mat& bgr, Rect area, bool isDrag) {
    return impl_->ReadLines(bgr, area, isDrag);
}
