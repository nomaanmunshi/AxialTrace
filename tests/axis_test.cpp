#include "axis.hpp"
#include <gtest/gtest.h>
#include <sstream>
#include <limits>

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

TEST(Motion, ExtremeCommandsDoNotOverflowDuringReversal) {
    Actuator axis;
    axis.position = 50000;
    axis.velocity = 1000;
    axis.step(std::numeric_limits<int>::min(), false);
    EXPECT_EQ(axis.velocity, 0);
    EXPECT_EQ(axis.position, 50000);
    axis.velocity = -1000;
    axis.step(std::numeric_limits<int>::max(), false);
    EXPECT_EQ(axis.velocity, 0);
    EXPECT_EQ(axis.position, 50000);
}

TEST(Control, SlowFaultDeadlineIncludesEarlierQuietReadings) {
    for (const auto scenario : {"jam", "frozen"}) {
        const auto rows = simulate(scenario, 7, 32, 100);
        EXPECT_EQ(rows[5].encoder, 1);
        EXPECT_FALSE(rows[16].stopped);
        EXPECT_TRUE(rows[17].stopped);
        EXPECT_EQ(rows[17].command, 0);
    }
}

TEST(Control, QuantizationIsToleratedInBothDirectionsAtEverySupportedSpeed) {
    for (int speed = 100; speed <= 1000; ++speed) {
        for (int phase = 0; phase < 1000; phase += 100) {
            for (const int target : {20, 80}) {
                Actuator axis;
                axis.position = 50000 + phase;
                Encoder encoder;
                Controller controller(speed);
                for (int tick = 0; tick < 120; ++tick) {
                    const int command = controller.update(target, encoder.sample(axis.position, false));
                    ASSERT_FALSE(controller.stopped()) << speed << " " << phase << " " << target;
                    axis.step(command, false);
                }
            }
        }
    }
}

TEST(Control, FaultLatchHoldsAfterMeasurementsRecover) {
    Controller controller;
    for (int tick = 0; tick <= 12; ++tick) controller.update(80, 0);
    ASSERT_TRUE(controller.stopped());
    EXPECT_EQ(controller.update(0, 20), 0);
    EXPECT_EQ(controller.update(80, 30), 0);
    EXPECT_TRUE(controller.stopped());
}

TEST(Control, FaultAfterArrivalDoesNotTriggerFalseStop) {
    for (const auto scenario : {"jam", "frozen"}) {
        const auto rows = simulate(scenario, 90, 128);
        for (const auto& row : rows) EXPECT_FALSE(row.stopped);
        EXPECT_EQ(rows.back().encoder, 80);
    }
}
