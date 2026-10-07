#include "Shared/PlayerActions.hpp"

#include <gtest/gtest.h>

TEST(PlayerActions, DirectionAndInvalidPlayerDefaults) {
    SPlayerActionState action{.up = true, .right = true};

    EXPECT_FLOAT_EQ(action.horizontal(), 1.0F);
    EXPECT_FLOAT_EQ(action.vertical(), -1.0F);

    SPLayerActions actions;
    actions.states.at(0) = action;
    EXPECT_EQ(&actions.forPlayer(0), &actions.states.at(0));
    const auto& invalid = actions.forPlayer(SPLayerActions::MaxPlayers);
    EXPECT_FALSE(invalid.up);
    EXPECT_FALSE(invalid.down);
    EXPECT_FALSE(invalid.left);
    EXPECT_FALSE(invalid.right);
    EXPECT_FALSE(invalid.fire);
}
