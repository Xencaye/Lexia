#include "tts/speech_key.h"

// The Speech key (F6). Taps that come within the tap window of the one before are counted together:
//   1st tap:  speech goes quiet at once (Pause), so the user hears that the key worked;
//   3rd tap:  Stop (the queue is emptied), at once;
//   else, when the window runs out: one Skip per tap (1 tap = next Block, 2 taps = the Block after).
// Time is passed in, so the tests need no clock. Each call returns how many ms are left in the
// window (0 = nothing waits), which the App uses to set its window timer.

long long SpeechKey::Tap(long long nowMs) {
    Settle(nowMs);  // a window that ran out belongs to the earlier taps
    if (!queue_.Active()) {  // nothing to speak or pause: a tap does nothing
        taps_ = 0;
        return 0;
    }
    if (taps_ + 1 >= kStopTaps) {  // the Stop tap, in time
        taps_ = 0;
        queue_.Stop();
        return 0;
    }
    if (taps_ == 0 && !queue_.Paused()) queue_.Pause();  // the first tap goes quiet at once
    ++taps_;
    lastTapMs_ = nowMs;
    return windowMs_;
}

long long SpeechKey::Settle(long long nowMs) {
    if (taps_ == 0) return 0;
    const long long left = windowMs_ - (nowMs - lastTapMs_);
    if (left > 0) return left;
    SkipTapped();
    return 0;
}

void SpeechKey::Flush() {
    if (taps_ == 0) return;
    SkipTapped();  // as Settle does when the window runs out
}

void SpeechKey::SkipTapped() {
    const int n = taps_;
    taps_ = 0;
    for (int i = 0; i < n; ++i) queue_.Skip();  // the first also ends the pause: the next Block plays
}
