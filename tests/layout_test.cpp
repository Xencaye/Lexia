// Unit tests for layout.h, through its public interface only, with hand-made Lines.
// Expected values come from reading the rules in layout.cpp; each edge case sits one pixel
// inside or outside a threshold so a change to the threshold shows up here.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "layout.h"

namespace {

TextLine MakeLine(int left, int top, int right, int bottom, const std::string& text,
                  float conf = 90.f) {
    TextLine l;
    l.box = {left, top, right, bottom};
    l.text = text;
    l.conf = conf;
    return l;
}

TextBlock MakeBlock(int left, int top, int right, int bottom, const std::string& text = "") {
    TextBlock b;
    b.box = {left, top, right, bottom};
    b.text = text;
    return b;
}

}  // namespace

// ------------------------------------------------------------ SortReadingOrder ---

TEST_CASE("SortReadingOrder: rows top to bottom, left to right inside a row (row order)") {
    std::vector<TextLine> v = {
        MakeLine(0, 100, 50, 120, "row2-left"),
        MakeLine(100, 0, 150, 20, "row1-right"),
        MakeLine(0, 0, 50, 20, "row1-left"),
    };
    SortReadingOrder(v);
    REQUIRE(v.size() == 3);
    CHECK(v[0].text == "row1-left");
    CHECK(v[1].text == "row1-right");
    CHECK(v[2].text == "row2-left");
}

TEST_CASE("SortReadingOrder: a piece whose top edge jitters by 2 px stays in its row (row order)") {
    // The right piece's top is above the left piece's, but they are one row.
    std::vector<TextLine> v = {
        MakeLine(100, 100, 150, 120, "right"),
        MakeLine(0, 102, 50, 122, "left"),
    };
    SortReadingOrder(v);
    REQUIRE(v.size() == 2);
    CHECK(v[0].text == "left");
    CHECK(v[1].text == "right");
}

TEST_CASE("SortReadingOrder: same row needs a vertical overlap of half the shorter box (row order)") {
    // Anchor is 0..20 (height 20), on the right. A candidate on the left is in the same
    // row, and so is listed first, when it overlaps by at least 10 px.
    SUBCASE("overlap of 10 px is the same row") {
        std::vector<TextLine> v = {MakeLine(100, 0, 150, 20, "anchor"),
                                   MakeLine(0, 10, 50, 30, "left")};
        SortReadingOrder(v);
        CHECK(v[0].text == "left");
        CHECK(v[1].text == "anchor");
    }
    SUBCASE("overlap of 9 px is the next row") {
        std::vector<TextLine> v = {MakeLine(100, 0, 150, 20, "anchor"),
                                   MakeLine(0, 11, 50, 31, "left")};
        SortReadingOrder(v);
        CHECK(v[0].text == "anchor");
        CHECK(v[1].text == "left");
    }
}

TEST_CASE("SortReadingOrder: sorts TextBlocks the same way (row order)") {
    std::vector<TextBlock> v = {
        MakeBlock(0, 100, 50, 120, "below"),
        MakeBlock(100, 0, 150, 20, "right"),
        MakeBlock(0, 0, 50, 20, "left"),
    };
    SortReadingOrder(v);
    REQUIRE(v.size() == 3);
    CHECK(v[0].text == "left");
    CHECK(v[1].text == "right");
    CHECK(v[2].text == "below");
}

TEST_CASE("SortReadingOrder: empty and single-item lists are unchanged") {
    std::vector<TextLine> none;
    SortReadingOrder(none);
    CHECK(none.empty());

    std::vector<TextLine> one = {MakeLine(5, 5, 10, 10, "only")};
    SortReadingOrder(one);
    REQUIRE(one.size() == 1);
    CHECK(one[0].text == "only");
}

// ---------------------------------------------------------------- GroupBlocks ---
// Lines below are 20 px tall unless noted, so the line height `lh` is 20: the first gap of a
// Block may be up to 0.9 * lh = 18 px, a Line may be indented by up to 1.5 * lh = 30 px, and a
// Line that goes on into the next one must be at least 8 * lh = 160 px wide (tests use 200 px).

TEST_CASE("GroupBlocks: no Lines gives no Blocks") {
    CHECK(GroupBlocks({}).empty());
}

TEST_CASE("GroupBlocks: close Lines merge into one Block") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 300, 20, "The quick brown"),
        MakeLine(0, 25, 300, 45, "fox jumps over"),
        MakeLine(0, 50, 300, 70, "the lazy dog."),
    });
    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].text == "The quick brown fox jumps over the lazy dog.");
    CHECK(blocks[0].box.left == 0);
    CHECK(blocks[0].box.top == 0);
    CHECK(blocks[0].box.right == 300);
    CHECK(blocks[0].box.bottom == 70);
}

TEST_CASE("GroupBlocks: Lines are merged in reading order whatever order they arrive in") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 50, 300, 70, "three"),
        MakeLine(0, 0, 300, 20, "one"),
        MakeLine(0, 25, 300, 45, "two"),
    });
    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].text == "one two three");
}

TEST_CASE("GroupBlocks: a Block keeps the Lines it was built from, in reading order") {
    // Two columns arriving interleaved and shuffled: each Block must hold its own column's
    // Lines (box, text and confidence untouched), top to bottom, and a hyphenated Line keeps
    // its own text even though the Block text is re-joined.
    const auto blocks = GroupBlocks({
        MakeLine(300, 25, 500, 45, "c2b", 71.f),
        MakeLine(0, 25, 200, 45, "inter-", 83.f),
        MakeLine(0, 50, 200, 70, "national", 77.f),
        MakeLine(300, 0, 500, 20, "c2a", 66.f),
        MakeLine(0, 0, 200, 20, "c1a", 95.f),
    });
    REQUIRE(blocks.size() == 2);
    REQUIRE(blocks[0].lines.size() == 3);
    CHECK(blocks[0].lines[0].text == "c1a");
    CHECK(blocks[0].lines[1].text == "inter-");
    CHECK(blocks[0].lines[2].text == "national");
    CHECK(blocks[0].lines[1].conf == doctest::Approx(83.f));
    CHECK(blocks[0].lines[1].box.left == 0);
    CHECK(blocks[0].lines[1].box.top == 25);
    CHECK(blocks[0].lines[1].box.right == 200);
    CHECK(blocks[0].lines[1].box.bottom == 45);
    REQUIRE(blocks[1].lines.size() == 2);
    CHECK(blocks[1].lines[0].text == "c2a");
    CHECK(blocks[1].lines[1].text == "c2b");
}

TEST_CASE("GroupBlocks: a dropped Line is in no Block's Lines") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 300, 20, "kept"),
        MakeLine(0, 25, 300, 45, "noise", 10.f),
    });
    REQUIRE(blocks.size() == 1);
    REQUIRE(blocks[0].lines.size() == 1);
    CHECK(blocks[0].lines[0].text == "kept");
}

TEST_CASE("GroupBlocks: a paragraph gap splits Blocks") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 300, 20, "First paragraph."),
        MakeLine(0, 60, 300, 80, "Second paragraph."),  // gap 40 > 18
    });
    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].text == "First paragraph.");
    CHECK(blocks[1].text == "Second paragraph.");
}

TEST_CASE("GroupBlocks: the first gap in a Block may be up to 0.9 line heights") {
    SUBCASE("17 px joins") {
        CHECK(GroupBlocks({MakeLine(0, 0, 300, 20, "a"), MakeLine(0, 37, 300, 57, "b")}).size() == 1);
    }
    SUBCASE("19 px splits") {
        CHECK(GroupBlocks({MakeLine(0, 0, 300, 20, "a"), MakeLine(0, 39, 300, 59, "b")}).size() == 2);
    }
}

TEST_CASE("GroupBlocks: once a Block has a gap, the next gap is judged against it") {
    // Gap 5 between the first two Lines, so the typical gap is 5 and the limit becomes
    // max(5 * 1.8, 5 + 0.5 * 20) = 15 (not the 18 allowed for a first gap).
    SUBCASE("a 14 px gap joins") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 300, 20, "a"), MakeLine(0, 25, 300, 45, "b"),
                                         MakeLine(0, 59, 300, 79, "c")});
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].text == "a b c");
    }
    SUBCASE("a 16 px gap splits") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 300, 20, "a"), MakeLine(0, 25, 300, 45, "b"),
                                         MakeLine(0, 61, 300, 81, "c")});
        REQUIRE(blocks.size() == 2);
        CHECK(blocks[0].text == "a b");
        CHECK(blocks[1].text == "c");
    }
}

TEST_CASE("GroupBlocks: a heading in bigger text is its own Block") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 300, 40, "Heading"),  // 40 px tall against 20 px: ratio 2, over 1.7
        MakeLine(0, 45, 300, 65, "Body text."),
    });
    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].text == "Heading");
    CHECK(blocks[1].text == "Body text.");
}

TEST_CASE("GroupBlocks: Lines side by side are separate Blocks, left one first") {
    const auto blocks = GroupBlocks({
        MakeLine(200, 0, 300, 20, "right"),
        MakeLine(0, 0, 100, 20, "left"),
    });
    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].text == "left");
    CHECK(blocks[1].text == "right");
}

TEST_CASE("GroupBlocks: two columns of two Lines come out as two Blocks") {
    // Row order interleaves the columns; grouping must still keep each column together.
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 200, 20, "c1a"),
        MakeLine(300, 0, 500, 20, "c2a"),
        MakeLine(0, 25, 200, 45, "c1b"),
        MakeLine(300, 25, 500, 45, "c2b"),
    });
    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].text == "c1a c1b");
    CHECK(blocks[1].text == "c2a c2b");
}

TEST_CASE("GroupBlocks: a Line indented by up to 1.5 line heights still joins") {
    SUBCASE("29 px indent joins") {
        CHECK(GroupBlocks({MakeLine(0, 0, 300, 20, "a"), MakeLine(29, 25, 329, 45, "b")}).size() == 1);
    }
    SUBCASE("31 px indent splits") {
        CHECK(GroupBlocks({MakeLine(0, 0, 300, 20, "a"), MakeLine(31, 25, 331, 45, "b")}).size() == 2);
    }
}

TEST_CASE("GroupBlocks: centred or right-aligned Lines join although the left edges differ") {
    SUBCASE("centred") {
        CHECK(GroupBlocks({MakeLine(0, 0, 300, 20, "a"), MakeLine(50, 25, 250, 45, "b")}).size() == 1);
    }
    SUBCASE("right-aligned") {
        CHECK(GroupBlocks({MakeLine(0, 0, 300, 20, "a"), MakeLine(100, 25, 300, 45, "b")}).size() == 1);
    }
}

TEST_CASE("GroupBlocks: short stacked Lines are a list, not a paragraph") {
    // A Line that continues into the next one must be at least 8 line heights (160 px) wide:
    // a wrapped paragraph's Lines are long, a menu's entries are short.
    SUBCASE("three 100 px entries are three Blocks") {
        const auto blocks = GroupBlocks({
            MakeLine(0, 0, 100, 20, "Save slot 1"),
            MakeLine(0, 25, 100, 45, "Save slot 2"),
            MakeLine(0, 50, 100, 70, "Quit game"),
        });
        REQUIRE(blocks.size() == 3);
        CHECK(blocks[0].text == "Save slot 1");
        CHECK(blocks[2].text == "Quit game");
    }
    SUBCASE("a 160 px Line continues") {
        CHECK(GroupBlocks({MakeLine(0, 0, 160, 20, "a"), MakeLine(0, 25, 100, 45, "b")}).size() == 1);
    }
    SUBCASE("a 159 px Line does not") {
        CHECK(GroupBlocks({MakeLine(0, 0, 159, 20, "A"), MakeLine(0, 25, 100, 45, "B")}).size() == 2);
    }
    SUBCASE("the last Line of a paragraph may be short") {
        const auto blocks = GroupBlocks({
            MakeLine(0, 0, 300, 20, "A full first line"),
            MakeLine(0, 25, 300, 45, "and a full second line"),
            MakeLine(0, 50, 40, 70, "end."),
        });
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].text == "A full first line and a full second line end.");
    }
}

TEST_CASE("GroupBlocks: overlapping Lines are one run of text however short") {
    // Big lettering in a narrow box: the Lines' boxes overlap (gap below 0), which list entries
    // with padding never do.
    SUBCASE("a gap of -1 joins") {
        CHECK(GroupBlocks({MakeLine(0, 0, 100, 20, "WHAT"), MakeLine(0, 19, 100, 39, "ABOUT")}).size() == 1);
    }
    SUBCASE("a gap of 0 splits") {
        CHECK(GroupBlocks({MakeLine(0, 0, 100, 20, "WHAT"), MakeLine(0, 20, 100, 40, "ABOUT")}).size() == 2);
    }
}

TEST_CASE("GroupBlocks: Lines under confidence 40 are dropped") {
    SUBCASE("40 stays") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 300, 20, "kept", 40.f)});
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].text == "kept");
    }
    SUBCASE("39 goes") {
        CHECK(GroupBlocks({MakeLine(0, 0, 300, 20, "noise", 39.f)}).empty());
    }
}

TEST_CASE("GroupBlocks: Lines without a letter or digit are dropped") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 300, 20, "--- ***"),
        MakeLine(0, 25, 300, 45, "Page 7"),
    });
    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].text == "Page 7");
}

TEST_CASE("GroupBlocks: a dropped Line does not bridge the Lines around it") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 300, 20, "one"),
        MakeLine(0, 25, 300, 45, "~~"),
        MakeLine(0, 50, 300, 70, "two"),
    });
    // The junk Line is gone before grouping, so "one" and "two" are 30 px apart: too far.
    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].text == "one");
    CHECK(blocks[1].text == "two");
}

// ---- Reading order of the Blocks (XY cut). Lines are 20 px tall, so lh = 20: a horizontal strip
// cuts from 5 px (0.25 lh), a vertical one from 100 px (5 lh), or from 30 px (1.5 lh) when no
// horizontal strip exists. Every Line below is 60 px wide and far from its neighbours, so each
// is its own Block.

TEST_CASE("GroupBlocks: a column across a wide gutter is read down before the next column") {
    // Sidebar rows at 0, 50, 100 and main rows at 25, 75 (no vertical overlap): row order would
    // interleave them; a 140 px (7 lh) gutter makes them two columns.
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 60, 20, "s1"),     MakeLine(200, 25, 260, 45, "m1"),
        MakeLine(0, 50, 60, 70, "s2"),    MakeLine(200, 75, 260, 95, "m2"),
        MakeLine(0, 100, 60, 120, "s3"),
    });
    REQUIRE(blocks.size() == 5);
    CHECK(blocks[0].text == "s1");
    CHECK(blocks[1].text == "s2");
    CHECK(blocks[2].text == "s3");
    CHECK(blocks[3].text == "m1");
    CHECK(blocks[4].text == "m2");
}

TEST_CASE("GroupBlocks: a gutter of 99 px is too narrow to beat the rows") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 60, 20, "s1"),     MakeLine(159, 30, 219, 50, "m1"),
        MakeLine(0, 60, 60, 80, "s2"),
    });
    REQUIRE(blocks.size() == 3);
    CHECK(blocks[0].text == "s1");
    CHECK(blocks[1].text == "m1");
    CHECK(blocks[2].text == "s2");
}

TEST_CASE("GroupBlocks: a gutter of 100 px (5 lh) makes two columns") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 60, 20, "s1"),     MakeLine(160, 30, 220, 50, "m1"),
        MakeLine(0, 60, 60, 80, "s2"),
    });
    REQUIRE(blocks.size() == 3);
    CHECK(blocks[0].text == "s1");
    CHECK(blocks[1].text == "s2");
    CHECK(blocks[2].text == "m1");
}

TEST_CASE("GroupBlocks: a table with a narrow gutter is read row by row") {
    // Labels and values 60 px (3 lh) apart, rows 40 px apart: the row strips (20 px) are the
    // widest cuts.
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 60, 20, "label1"),   MakeLine(120, 0, 180, 20, "value1"),
        MakeLine(0, 40, 60, 60, "label2"),  MakeLine(120, 40, 180, 60, "value2"),
    });
    REQUIRE(blocks.size() == 4);
    CHECK(blocks[0].text == "label1");
    CHECK(blocks[1].text == "value1");
    CHECK(blocks[2].text == "label2");
    CHECK(blocks[3].text == "value2");
}

TEST_CASE("GroupBlocks: a heading above two columns is read first, then each column") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 100, 60, 120, "c1a"),   MakeLine(300, 100, 360, 120, "c2a"),
        MakeLine(0, 0, 360, 20, "Heading text across the page"),
        MakeLine(0, 150, 60, 170, "c1b"),   MakeLine(300, 150, 360, 170, "c2b"),
    });
    REQUIRE(blocks.size() == 5);
    CHECK(blocks[0].text == "Heading text across the page");
    CHECK(blocks[1].text == "c1a");
    CHECK(blocks[2].text == "c1b");
    CHECK(blocks[3].text == "c2a");
    CHECK(blocks[4].text == "c2b");
}

TEST_CASE("GroupBlocks: a footer under two columns is read after both") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 60, 20, "c1"),   MakeLine(300, 0, 360, 20, "c2"),
        MakeLine(0, 200, 360, 220, "Footer text across the page"),
    });
    REQUIRE(blocks.size() == 3);
    CHECK(blocks[0].text == "c1");
    CHECK(blocks[1].text == "c2");
    CHECK(blocks[2].text == "Footer text across the page");
}

TEST_CASE("GroupBlocks: a 1-2 character Line under half the typical Line height is icon noise") {
    // Typical (median) Line height is 20 px. A stray "X" 9 px high is an icon the detector
    // read as a letter; one 10 px high is kept.
    SUBCASE("9 px high goes") {
        const auto blocks = GroupBlocks({
            MakeLine(0, 0, 300, 20, "A normal line of text"),
            MakeLine(0, 100, 12, 109, "X"),
            MakeLine(0, 200, 300, 220, "Another normal line"),
        });
        REQUIRE(blocks.size() == 2);
        CHECK(blocks[0].text == "A normal line of text");
        CHECK(blocks[1].text == "Another normal line");
    }
    SUBCASE("10 px high stays") {
        CHECK(GroupBlocks({
                  MakeLine(0, 0, 300, 20, "A normal line of text"),
                  MakeLine(0, 100, 12, 110, "X"),
                  MakeLine(0, 200, 300, 220, "Another normal line"),
              }).size() == 3);
    }
    SUBCASE("three characters stay however small") {
        CHECK(GroupBlocks({
                  MakeLine(0, 0, 300, 20, "A normal line of text"),
                  MakeLine(0, 100, 30, 109, "abc"),
                  MakeLine(0, 200, 300, 220, "Another normal line"),
              }).size() == 3);
    }
    SUBCASE("two Cyrillic letters (four bytes) are still two characters") {
        CHECK(GroupBlocks({
                  MakeLine(0, 0, 300, 20, "A normal line of text"),
                  MakeLine(0, 100, 20, 109, "\xD0\xB4\xD0\xB0"),
                  MakeLine(0, 200, 300, 220, "Another normal line"),
              }).size() == 2);
    }
    SUBCASE("a lone small Line is the typical Line, so it stays") {
        CHECK(GroupBlocks({MakeLine(0, 0, 12, 9, "X")}).size() == 1);
    }
}

TEST_CASE("LayoutParams: the defaults are the tuned values") {
    const LayoutParams p;
    CHECK(p.paragraphGap == doctest::Approx(0.9));
    CHECK(p.columnGap == doctest::Approx(5.0));
    CHECK(p.listLine == doctest::Approx(8.0));
}

TEST_CASE("LayoutParams: paragraphGap is the limit of the first gap, in line heights") {
    // Lines are 20 px tall; a 12 px gap is 0.6 line heights.
    const std::vector<TextLine> lines = {MakeLine(0, 0, 300, 20, "a"), MakeLine(0, 32, 300, 52, "b")};
    CHECK(GroupBlocks(lines).size() == 1);
    LayoutParams tight;
    tight.paragraphGap = 0.5;
    CHECK(GroupBlocks(lines, tight).size() == 2);
    LayoutParams loose;
    loose.paragraphGap = 0.6;
    CHECK(GroupBlocks(lines, loose).size() == 1);
}

TEST_CASE("LayoutParams: columnGap is the narrowest gutter that makes two columns, in line heights") {
    // Gutter 99 px = 4.95 line heights: rows by default, columns once columnGap is 4.9.
    const std::vector<TextLine> lines = {
        MakeLine(0, 0, 60, 20, "s1"), MakeLine(159, 30, 219, 50, "m1"), MakeLine(0, 60, 60, 80, "s2")};
    const auto rows = GroupBlocks(lines);
    REQUIRE(rows.size() == 3);
    CHECK(rows[1].text == "m1");
    LayoutParams narrow;
    narrow.columnGap = 4.9;
    const auto columns = GroupBlocks(lines, narrow);
    REQUIRE(columns.size() == 3);
    CHECK(columns[1].text == "s2");
    CHECK(columns[2].text == "m1");
}

TEST_CASE("LayoutParams: listLine is how wide a Line must be to go on into the next, in line heights") {
    const std::vector<TextLine> lines = {MakeLine(0, 0, 100, 20, "A"), MakeLine(0, 25, 100, 45, "B")};
    CHECK(GroupBlocks(lines).size() == 2);
    LayoutParams none;
    none.listLine = 0;
    CHECK(GroupBlocks(lines, none).size() == 1);
    LayoutParams five;
    five.listLine = 5;  // 100 px is exactly 5 line heights
    CHECK(GroupBlocks(lines, five).size() == 1);
}

TEST_CASE("GroupBlocks: a short wrapped paragraph stays one Block when the next Line continues the sentence") {
    // Narrower than 8 line heights (160 px), so the width rule alone would split them; the next
    // Line starting in lower case shows the sentence goes on.
    SUBCASE("a centred two-line subtitle") {
        const auto blocks = GroupBlocks({MakeLine(60, 0, 180, 20, "Thank you all for"),
                                         MakeLine(80, 25, 160, 45, "coming tonight.")});
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].text == "Thank you all for coming tonight.");
    }
    SUBCASE("a narrow speech bubble") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 120, 20, "I think we should"),
                                         MakeLine(0, 25, 120, 45, "go now, before it"),
                                         MakeLine(0, 50, 60, 70, "gets dark.")});
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].text == "I think we should go now, before it gets dark.");
    }
    SUBCASE("a short two-line dialogue") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 140, 20, "* You really did a"),
                                         MakeLine(0, 25, 100, 45, "number on him.")});
        REQUIRE(blocks.size() == 1);
    }
    SUBCASE("a Cyrillic continuation (lower case) joins") {
        CHECK(GroupBlocks({MakeLine(0, 0, 120, 20, "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"),
                           MakeLine(0, 25, 100, 45, "\xD0\xB4\xD1\x80\xD1\x83\xD0\xB3")})
                  .size() == 1);
    }
    SUBCASE("a Line starting with a capital or a digit is a new entry") {
        CHECK(GroupBlocks({MakeLine(0, 0, 120, 20, "Save slot"), MakeLine(0, 25, 120, 45, "Quit game")}).size() == 2);
        CHECK(GroupBlocks({MakeLine(0, 0, 120, 20, "Save slot"), MakeLine(0, 25, 120, 45, "2 players")}).size() == 2);
    }
}

TEST_CASE("GroupBlocks: kCutRow, a horizontal strip cuts from 0.25 line heights (5 px)") {
    // Two sidebar rows and a main row between them; with a 4 px strip the Blocks are one band and
    // the 60 px gutter (the narrow fallback) makes columns, with a 5 px strip the rows win.
    SUBCASE("5 px cuts: row by row") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 60, 20, "s1"), MakeLine(120, 25, 180, 45, "m1"),
                                         MakeLine(0, 50, 60, 70, "s2")});
        REQUIRE(blocks.size() == 3);
        CHECK(blocks[1].text == "m1");
    }
    SUBCASE("4 px does not: column first") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 60, 20, "s1"), MakeLine(120, 24, 180, 44, "m1"),
                                         MakeLine(0, 48, 60, 68, "s2")});
        REQUIRE(blocks.size() == 3);
        CHECK(blocks[1].text == "s2");
    }
}

TEST_CASE("GroupBlocks: kCutNarrow, a vertical strip of 1.5 line heights (30 px) is the last resort") {
    // R1 overlaps both L1 and L2 in y, so no horizontal strip exists, and the gutter is under
    // 5 line heights. Row order would give L1 R1 L2; a 30 px strip makes columns.
    SUBCASE("30 px cuts: left column first") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 60, 20, "L1"), MakeLine(90, 15, 150, 35, "R1"),
                                         MakeLine(0, 30, 60, 50, "L2")});
        REQUIRE(blocks.size() == 3);
        CHECK(blocks[0].text == "L1");
        CHECK(blocks[1].text == "L2");
        CHECK(blocks[2].text == "R1");
    }
    SUBCASE("29 px does not: row order") {
        const auto blocks = GroupBlocks({MakeLine(0, 0, 60, 20, "L1"), MakeLine(89, 15, 149, 35, "R1"),
                                         MakeLine(0, 30, 60, 50, "L2")});
        REQUIRE(blocks.size() == 3);
        CHECK(blocks[1].text == "R1");
    }
}

// -------------------------------------------------------------- BlocksToText ---

TEST_CASE("BlocksToText: no Blocks gives an empty string") {
    CHECK(BlocksToText({}).empty());
}

TEST_CASE("BlocksToText: one Block is its text") {
    CHECK(BlocksToText({MakeBlock(0, 0, 10, 10, "alone")}) == "alone");
}

TEST_CASE("BlocksToText: Blocks are joined with a single space, in the order given") {
    const std::vector<TextBlock> blocks = {MakeBlock(0, 0, 10, 10, "one"),
                                           MakeBlock(0, 20, 10, 30, "two"),
                                           MakeBlock(0, 40, 10, 50, "three")};
    CHECK(BlocksToText(blocks) == "one two three");
}

TEST_CASE("Hyphen re-join (Latin): a word hyphenated across Lines is joined, then read as text") {
    const auto blocks = GroupBlocks({
        MakeLine(0, 0, 300, 20, "an inter-"),
        MakeLine(0, 25, 300, 45, "national word"),
    });
    CHECK(BlocksToText(blocks) == "an international word");
}

TEST_CASE("Hyphen re-join (Latin): only when the next Line starts with a lowercase letter") {
    SUBCASE("capital letter keeps the hyphen and a space") {
        CHECK(BlocksToText(GroupBlocks({MakeLine(0, 0, 300, 20, "a well-"),
                                        MakeLine(0, 25, 300, 45, "Known name")})) ==
              "a well- Known name");
    }
    SUBCASE("digit keeps the hyphen and a space") {
        CHECK(BlocksToText(GroupBlocks({MakeLine(0, 0, 300, 20, "section 4-"),
                                        MakeLine(0, 25, 300, 45, "2 applies")})) ==
              "section 4- 2 applies");
    }
}

TEST_CASE("Hyphen re-join (Cyrillic): a Bulgarian word hyphenated across Lines is joined (R27)") {
    CHECK(BlocksToText(GroupBlocks({MakeLine(0, 0, 300, 20, "пре-"),
                                    MakeLine(0, 25, 300, 45, "ход")})) == "преход");
    CHECK(BlocksToText(GroupBlocks({MakeLine(0, 0, 300, 20, "междуна-"),
                                    MakeLine(0, 25, 300, 45, "родно")})) == "международно");
    SUBCASE("capital letter keeps the hyphen and a space") {
        CHECK(BlocksToText(GroupBlocks({MakeLine(0, 0, 300, 20, "пре-"),
                                        MakeLine(0, 25, 300, 45, "Ход")})) == "пре- Ход");
    }
}

TEST_CASE("Hyphen re-join (Latin-1): a lowercase accented letter on the next Line joins too") {
    CHECK(BlocksToText(GroupBlocks({MakeLine(0, 0, 300, 20, "a na-"),
                                    MakeLine(0, 25, 300, 45, "ïve view")})) == "a naïve view");
}

// ---------------------------------------------------------------- FindBlockAt ---
// The block below is x 100..200, y 100..150; clicks up to 8 px outside it still count.

TEST_CASE("FindBlockAt: an empty list finds nothing") {
    CHECK(FindBlockAt({}, Point{10, 10}) == -1);
}

TEST_CASE("FindBlockAt: a point inside a Block finds it") {
    const std::vector<TextBlock> blocks = {MakeBlock(0, 0, 50, 50), MakeBlock(100, 100, 200, 150)};
    CHECK(FindBlockAt(blocks, Point{150, 125}) == 1);
}

TEST_CASE("FindBlockAt: a point far outside every Block finds nothing") {
    const std::vector<TextBlock> blocks = {MakeBlock(100, 100, 200, 150)};
    CHECK(FindBlockAt(blocks, Point{500, 500}) == -1);
    CHECK(FindBlockAt(blocks, Point{150, 400}) == -1);
}

TEST_CASE("FindBlockAt: the tolerance is 8 px on every side") {
    const std::vector<TextBlock> blocks = {MakeBlock(100, 100, 200, 150)};
    SUBCASE("left") {
        CHECK(FindBlockAt(blocks, Point{92, 125}) == 0);
        CHECK(FindBlockAt(blocks, Point{91, 125}) == -1);
    }
    SUBCASE("right") {
        CHECK(FindBlockAt(blocks, Point{208, 125}) == 0);
        CHECK(FindBlockAt(blocks, Point{209, 125}) == -1);
    }
    SUBCASE("top") {
        CHECK(FindBlockAt(blocks, Point{150, 92}) == 0);
        CHECK(FindBlockAt(blocks, Point{150, 91}) == -1);
    }
    SUBCASE("bottom") {
        CHECK(FindBlockAt(blocks, Point{150, 158}) == 0);
        CHECK(FindBlockAt(blocks, Point{150, 159}) == -1);
    }
}

TEST_CASE("FindBlockAt: the smallest of nested Blocks wins, whatever the order") {
    const TextBlock outer = MakeBlock(0, 0, 500, 500);
    const TextBlock inner = MakeBlock(100, 100, 200, 150);
    CHECK(FindBlockAt({outer, inner}, Point{150, 125}) == 1);
    CHECK(FindBlockAt({inner, outer}, Point{150, 125}) == 0);
    // Outside the inner one (and its tolerance) only the outer one is under the point.
    CHECK(FindBlockAt({inner, outer}, Point{300, 300}) == 1);
}

TEST_CASE("FindBlockAt: the caller can pass the tolerance (scaled for the DPI)") {
    const std::vector<TextBlock> blocks = {MakeBlock(100, 100, 200, 150)};
    CHECK(FindBlockAt(blocks, Point{84, 125}, 16) == 0);
    CHECK(FindBlockAt(blocks, Point{83, 125}, 16) == -1);
    CHECK(FindBlockAt(blocks, Point{99, 125}, 0) == -1);
    CHECK(FindBlockAt(blocks, Point{100, 125}, 0) == 0);
}
