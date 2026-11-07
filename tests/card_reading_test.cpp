#include <cstdint>
#include <set>

#include "card_reading.h"
#include <doctest/doctest.h>

namespace {

// A stand-in for Speaker::HasReading: the ids that are "still queued".
struct FakeQueue {
    std::set<uint64_t> active;
    CardReading::IsActive Fn() {
        return [this](uint64_t id) { return active.count(id) != 0; };
    }
};

}  // namespace

TEST_CASE("CardReading: nothing is read at the start, and a press starts") {
    CardReading r;
    FakeQueue q;
    CHECK_FALSE(r.Reading());
    CHECK(r.Card().empty());
    CHECK(r.OnPress("read", q.Fn()) == CardReading::Press::Start);
}

TEST_CASE("CardReading: a press on the card that is read stops it, a press on another starts") {
    CardReading r;
    FakeQueue q;
    q.active = {7};
    r.Started("read", 7);
    CHECK(r.Reading());
    CHECK(r.Card() == "read");
    CHECK(r.OnPress("read", q.Fn()) == CardReading::Press::Stop);
    CHECK(r.OnPress("pause", q.Fn()) == CardReading::Press::Start);  // another card: it replaces the reading
    CHECK(r.Card() == "read");  // OnPress changes nothing by itself
}

TEST_CASE("CardReading: a press after the reading ended (not yet noticed) starts again") {
    CardReading r;
    FakeQueue q;
    r.Started("read", 3);  // the queue no longer has Reading 3
    CHECK(r.OnPress("read", q.Fn()) == CardReading::Press::Start);
}

TEST_CASE("CardReading: Started replaces the card, and id 0 (nothing queued) means no reading") {
    CardReading r;
    r.Started("read", 1);
    r.Started("runAsAdmin", 2);
    CHECK(r.Card() == "runAsAdmin");
    CHECK(r.Id() == 2);
    r.Started("pause", 0);
    CHECK_FALSE(r.Reading());
    CHECK(r.Card().empty());
}

TEST_CASE("CardReading: Update forgets a reading that is gone, once, and keeps one that goes on") {
    CardReading r;
    FakeQueue q;
    CHECK_FALSE(r.Update(q.Fn()));  // nothing held: nothing changes
    q.active = {5};
    r.Started("speech", 5);
    CHECK_FALSE(r.Update(q.Fn()));
    CHECK(r.Reading());
    q.active.clear();  // ended by itself, or stopped by the tray, the Control bar or the Speech key
    CHECK(r.Update(q.Fn()));
    CHECK_FALSE(r.Reading());
    CHECK(r.Card().empty());
    CHECK_FALSE(r.Update(q.Fn()));  // already forgotten: no second "changed"
}

TEST_CASE("CardReading: Clear forgets the reading") {
    CardReading r;
    r.Started("read", 4);
    r.Clear();
    CHECK_FALSE(r.Reading());
    CHECK(r.Card().empty());
    CHECK(r.Id() == 0);
}
