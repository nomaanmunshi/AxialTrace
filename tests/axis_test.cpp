#include "axis.hpp"
#include <gtest/gtest.h>
#include <sstream>

TEST(Motion, ReversalRespectsAcceleration) {
    Actuator axis;
    axis.step(4, false);
    axis.step(4, false);
    axis.step(-4, false);
    EXPECT_EQ(axis.velocity, 1);
    axis.step(-4, false);
    EXPECT_EQ(axis.velocity, 0);
    axis.step(-4, false);
    EXPECT_EQ(axis.velocity, -1);
}

TEST(Motion, TravelLimitsHoldAtBothEnds) {
    Actuator axis;
    for (int i = 0; i < 50; ++i) axis.step(4, false);
    EXPECT_EQ(axis.position, 100);
    EXPECT_EQ(axis.velocity, 0);
    for (int i = 0; i < 100; ++i) axis.step(-4, false);
    EXPECT_EQ(axis.position, 0);
    EXPECT_EQ(axis.velocity, 0);
}

TEST(Control, ReachesTargetAndHolds) {
    Actuator axis;
    Encoder encoder;
    Controller controller;
    for (int i = 0; i < 50; ++i) {
        const int command = controller.update(40, encoder.sample(axis.position, false));
        axis.step(command, false);
    }
    EXPECT_EQ(axis.position, 40);
    EXPECT_EQ(axis.velocity, 0);
    EXPECT_FALSE(controller.stopped());
}

TEST(Control, JamAndFreezeHaveIdenticalFeedback) {
    const auto jam = simulate("jam");
    const auto freeze = simulate("frozen");
    ASSERT_EQ(jam.size(), freeze.size());
    for (size_t i = 0; i < jam.size(); ++i) {
        EXPECT_EQ(jam[i].encoder, freeze[i].encoder);
        EXPECT_EQ(jam[i].command, freeze[i].command);
        EXPECT_EQ(jam[i].stopped, i >= 11);
        EXPECT_EQ(freeze[i].stopped, jam[i].stopped);
    }
    EXPECT_GT(freeze.back().position, jam.back().position);
}

TEST(Trace, ForwardOutputRepeatsByteForByte) {
    std::ostringstream first, second;
    write_trace(first, simulate("forward"));
    write_trace(second, simulate("forward"));
    EXPECT_EQ(first.str(), second.str());
    EXPECT_GT(simulate("forward").back().position, 0);
}
