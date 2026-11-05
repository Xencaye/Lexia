#pragma once
// The Speech key (F6): taps within about 0.4 s of each other are counted. When the window ends
// after one tap that is one Skip, after two taps two Skips; the third tap is Stop, at once.
// Win32-free; the caller supplies the time and a timer, so the tests need neither.
//
// The window runs from the last tap. A tap can't be told from the first part of a longer run
// until the window is over. To avoid both a late Skip and a Skip that a Stop would throw away,
// the first tap goes quiet at once (Pause) and the Skips themselves wait for the window to end:
//   - the Block stops being heard on the tap, not 0.4 s later;
//   - three taps leave the next Block unstarted: Stop empties the queue, nothing was skipped;
//   - a single tap's next Block starts 0.4 s later than a bare Skip would, the price of the above.
// A queue the person had paused stays paused during the window; Skip then resumes it (the queue's
// own rule), and three taps stop it. A Session starting during the window does the Skips at
// once (Flush), so the rest of the Reading is kept. A tap after a Stop starts a new count.
//
// UI thread only (the queue itself is safe from any thread).
#include "tts/reading_queue.h"

class SpeechKey {
public:
    static constexpr long long kTapWindowMs = 400;
    static constexpr int kStopTaps = 3;  // this many taps in a row are Stop

    explicit SpeechKey(ReadingQueue& queue, long long tapWindowMs = kTapWindowMs)
        : queue_(queue), windowMs_(tapWindowMs) {}

    // The key was pressed at `nowMs` (any steady clock in milliseconds). Returns how long until
    // Settle should be called, or 0 if nothing is waiting. A tap while the queue is empty is
    // ignored (a press queued just before the speech ended).
    long long Tap(long long nowMs);

    // Call when the time Tap returned has passed: a window that has run out becomes its Skips
    // (one per tap). Returns the time still to wait (a timer that fired a little early), or 0.
    long long Settle(long long nowMs);

    // Forget waiting taps, because something else emptied the queue (Stop, a new Reading
    // replacing it): a new Reading must not be skipped by them.
    void Cancel() { taps_ = 0; }

    // Waiting taps become their Skips now, without waiting for the window to end (as Settle
    // would do then). For a Session starting: the queue empties a paused queue then, and the
    // taps' own Pause must not count as the person's, which would drop the whole Reading. Nothing
    // if no tap is waiting.
    void Flush();

private:
    void SkipTapped();  // one Skip per waiting tap; clears the count

    ReadingQueue& queue_;
    long long windowMs_;
    int taps_ = 0;  // taps waiting to become Skips (0 = none)
    long long lastTapMs_ = 0;
};
