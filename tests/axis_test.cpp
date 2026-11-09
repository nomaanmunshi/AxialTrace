#include "axis.hpp"
#include <gtest/gtest.h>
#include <sstream>

TEST(Motion, ReversalRespectsAcceleration) {
    Actuator axis;
    axis.step(4000, false);
    axis.step(4000, false);
    axis.step(-4000, false);
    EXPECT_EQ(axis.velocity, 1000);
    axis.step(-4000, false);
    EXPECT_EQ(axis.velocity, 0);
    axis.step(-4000, false);
    EXPECT_EQ(axis.velocity, -1000);
}

TEST(Motion, TravelLimitsHoldAtBothEnds) {
    Actuator axis;
    for (int i = 0; i < 50; ++i) axis.step(4000, false);
    EXPECT_EQ(axis.position, 100000);
    EXPECT_EQ(axis.velocity, 0);
    for (int i = 0; i < 100; ++i) axis.step(-4000, false);
    EXPECT_EQ(axis.position, 0);
    EXPECT_EQ(axis.velocity, 0);
}

TEST(Control, ReachesTargetAndHolds) {
    const auto rows = simulate("arrival");
    EXPECT_EQ(rows.back().position, 12000);
    EXPECT_EQ(rows.back().velocity, 0);
    EXPECT_FALSE(rows.back().stopped);
}

TEST(Control, JamAndFreezeHaveIdenticalFeedback) {
    const auto jam = simulate("jam");
    const auto freeze = simulate("frozen");
    ASSERT_EQ(jam.size(), freeze.size());
    for (size_t i = 0; i < jam.size(); ++i) {
        EXPECT_EQ(jam[i].encoder, freeze[i].encoder);
        EXPECT_EQ(jam[i].command, freeze[i].command);
        EXPECT_EQ(jam[i].stopped, i >= 19);
        EXPECT_EQ(freeze[i].stopped, jam[i].stopped);
    }
    EXPECT_GT(freeze.back().position, jam.back().position);
}

TEST(Control, FaultStartMovesDetectionDeadline) {
    for (const int fault_tick : {0, 1, 7, 20}) {
        for (const auto scenario : {"jam", "frozen"}) {
            const auto rows = simulate(scenario, fault_tick);
            for (const auto& row : rows)
                EXPECT_EQ(row.stopped, row.tick >= fault_tick + 12);
        }
    }
}

TEST(Control, QuantizedSlowMotionDoesNotTriggerStop) {
    const auto rows = simulate("slow");
    EXPECT_EQ(rows[1].encoder, rows[2].encoder);
    EXPECT_GT(rows[2].position, rows[1].position);
    for (const auto& row : rows) EXPECT_FALSE(row.stopped);
    EXPECT_EQ(rows.back().encoder, 2);
}

TEST(Encoder, RoundsToNearestCountAndFreezesLastReading) {
    Encoder encoder;
    EXPECT_EQ(encoder.sample(1499, false), 1);
    EXPECT_EQ(encoder.sample(1500, false), 2);
    EXPECT_EQ(encoder.sample(9999, true), 2);
}

TEST(Trace, EveryScenarioRepeatsByteForByte) {
    for (const auto scenario : {"forward", "reversal", "arrival", "limits", "jam", "frozen", "slow"}) {
        std::ostringstream first, second;
        write_trace(first, simulate(scenario));
        write_trace(second, simulate(scenario));
        EXPECT_EQ(first.str(), second.str()) << scenario;
    }
}
