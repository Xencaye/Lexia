// Unit tests for which keys are registered when (src/hotkeys.h; ADR-0001: a game must keep
// receiving F6 / F7 while Lexia is silent).
#include <doctest/doctest.h>

#include <initializer_list>

#include "hotkeys.h"

TEST_CASE("Keys: while On and silent only the Read and Language keys are taken") {
    CHECK(KeyWanted(HK_READ, /*on=*/true, /*speaking=*/false));
    CHECK(KeyWanted(HK_LANG, true, false));
    CHECK_FALSE(KeyWanted(HK_PAUSE, true, false));
    CHECK_FALSE(KeyWanted(HK_SPEECH, true, false));
}

TEST_CASE("Keys: while On and speaking (or paused) the Pause and Speech keys are taken too") {
    for (int id : {HK_READ, HK_PAUSE, HK_SPEECH, HK_LANG}) {
        CHECK(KeyWanted(id, true, true));
    }
}

TEST_CASE("Keys: while Off no key is taken, speaking or not") {
    for (bool speaking : {false, true}) {
        for (int id : {HK_READ, HK_PAUSE, HK_SPEECH, HK_LANG}) {
            CHECK_FALSE(KeyWanted(id, false, speaking));
        }
    }
}

TEST_CASE("Keys: Esc belongs to a Session, not to the standing keys") {
    CHECK_FALSE(KeyWanted(HK_ESC, true, true));
}
