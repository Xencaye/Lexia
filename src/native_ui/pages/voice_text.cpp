#include "native_ui/pages/voice_text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "strings/string_ids.h"
#include "strings/strings.h"

namespace nui {

namespace {

constexpr double kMegabyte = 1024.0 * 1024.0;

// JavaScript's Math.round: halves go up.
double RoundHalfUp(double v) { return std::floor(v + 0.5); }

// A number as JavaScript's String(n) writes the values of the sliders: no trailing zeros, no exponent.
std::string Plain(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s.empty() ? "0" : s;
}

std::string Fixed(double v, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

int DecimalsFor(uint64_t bytes) { return static_cast<double>(bytes) / kMegabyte < 10 ? 1 : 0; }

// Megabytes rounded to `decimals`, written with that many decimals.
std::string Megabytes(uint64_t bytes, int decimals) {
    const double f = std::pow(10.0, decimals);
    return Fixed(RoundHalfUp(static_cast<double>(bytes) / kMegabyte * f) / f, decimals);
}

}  // namespace

std::string VoiceControlText(const std::string& lang, const std::string& name, double value) {
    namespace id = strings::id;
    if (name == "speed") {
        std::string s = Plain(RoundHalfUp(value * 100) / 100);
        if (s.find('.') == std::string::npos) s += ".0";  // 1 reads "1.0×"
        return strings::Format(lang, id::kWindowVoiceSpeedValue, {{"value", s}});
    }
    if (name == "pitch") {
        const double n = RoundHalfUp(value * 10) / 10;
        // U+2212, the minus sign, not a hyphen: a screen reader says "minus".
        const char* sign = n > 0 ? "+" : (n < 0 ? "\xE2\x88\x92" : "");
        const bool one = std::fabs(n) == 1;
        const std::string s = Plain(std::fabs(n));  // "1", "1.5": one decimal at most, after the rounding above
        return strings::Format(lang, one ? id::kWindowVoicePitchValueOne : id::kWindowVoicePitchValue,
                               {{"value", sign + s}});
    }
    return strings::Format(lang, id::kWindowVoicePercentValue,
                           {{"value", std::to_string(static_cast<long long>(RoundHalfUp(value * 100)))}});
}

std::string DownloadSizeText(const std::string& lang, uint64_t bytes) {
    return strings::Format(lang, strings::id::kWindowDownloadSize, {{"mb", Megabytes(bytes, DecimalsFor(bytes))}});
}

std::string DownloadProgressText(const std::string& lang, uint64_t done, uint64_t total) {
    const int d = DecimalsFor(total);
    const double f = std::pow(10.0, d);
    const double floored = std::floor(static_cast<double>(std::min(done, total)) / kMegabyte * f) / f;
    return strings::Format(lang, strings::id::kWindowDownloadProgress,
                           {{"done", Fixed(floored, d)}, {"total", Megabytes(total, d)}});
}

}  // namespace nui
