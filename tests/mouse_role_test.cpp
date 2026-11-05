// Unit tests for mouse_role.h: which button Picks and which Leaves.
#include <doctest/doctest.h>

#include <initializer_list>

#include "mouse_role.h"

TEST_CASE("Mouse role: right-handed, the left button Picks and the right button Leaves") {
    CHECK(RoleOf(MouseButton::Left, false) == MouseRole::Pick);
    CHECK(RoleOf(MouseButton::Right, false) == MouseRole::Leave);
}

TEST_CASE("Mouse role: left-handed swaps them") {
    CHECK(RoleOf(MouseButton::Left, true) == MouseRole::Leave);
    CHECK(RoleOf(MouseButton::Right, true) == MouseRole::Pick);
}

TEST_CASE("Mouse role: one button never has both roles") {
    for (const bool leftHanded : {false, true}) {
        CHECK(RoleOf(MouseButton::Left, leftHanded) != RoleOf(MouseButton::Right, leftHanded));
    }
}
