#include "axis.hpp"
#include "replay.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>

TEST(Replay, AcceptsEveryBaselineScenario) {
    for (const auto scenario : {"forward", "reversal", "arrival", "limits", "jam", "frozen", "slow"}) {
        std::ostringstream csv;
        write_trace(csv, simulate(scenario));
        std::istringstream input(csv.str());
        EXPECT_TRUE(verify(read_replay(input)).empty()) << scenario;
    }
}

TEST(Replay, NamesExpectedAndObservedStopTicks) {
    std::ifstream input(FIXTURE_DIR "/late_stop.csv");
    ASSERT_TRUE(input);
    const auto issues = verify(read_replay(input));
    ASSERT_FALSE(issues.empty());
    EXPECT_EQ(issues.front().tick, 4);
    EXPECT_EQ(issues.front().expected_stop, 4);
    EXPECT_EQ(issues.front().observed_stop, 5);
}

TEST(Replay, CorrectFixturePasses) {
    std::ifstream input(FIXTURE_DIR "/correct_stop.csv");
    ASSERT_TRUE(input);
    EXPECT_TRUE(verify(read_replay(input)).empty());
}

TEST(Replay, DoesNotUsePhysicsColumns) {
    auto samples = simulate("frozen");
    std::ostringstream original, changed;
    write_trace(original, samples);
    for (auto& sample : samples) {
        sample.position = 999999;
        sample.velocity = -999999;
    }
    write_trace(changed, samples);
    std::istringstream first(original.str()), second(changed.str());
    EXPECT_EQ(verify(read_replay(first)).size(), verify(read_replay(second)).size());
}

TEST(Replay, RejectsMissingRowsAndMalformedFields) {
    for (const auto body : {
        "tick,target,encoder,command,position,velocity,stopped\n",
        "tick,target,encoder,command,position,velocity,stopped\n0,80,0,1,0,0,0\n2,80,0,1,0,0,0\n",
        "tick,target,encoder,command,position,velocity,stopped\n0,80,0x,1,0,0,0\n",
        "tick,target,encoder,command,position,velocity,stopped\n0,80,0,1,0,0,2\n"}) {
        std::istringstream input(body);
        EXPECT_THROW(read_replay(input), std::runtime_error);
    }
}

TEST(Replay, RejectsEarlyStopAndDrivingAfterStop) {
    std::istringstream early(
        "tick,target,encoder,command,position,velocity,stopped\n0,80,0,0,0,0,1\n");
    EXPECT_FALSE(verify(read_replay(early)).empty());
    std::istringstream driving(
        "tick,target,encoder,command,position,velocity,stopped\n"
        "0,80,0,1,0,0,0\n1,80,0,1,0,0,0\n2,80,0,1,0,0,0\n"
        "3,80,0,1,0,0,0\n4,80,0,1,0,0,1\n");
    EXPECT_FALSE(verify(read_replay(driving)).empty());
}

TEST(Replay, CatchesOneTickDelayInLiveControllerOutput) {
    Actuator axis;
    Encoder encoder;
    Controller controller;
    std::vector<Sample> delayed;
    bool previous_stop = false;
    int previous_command = 0;
    for (int tick = 0; tick < 16; ++tick) {
        const int measured = encoder.sample(axis.position, false);
        const int command = controller.update(80, measured);
        const bool stopped = controller.stopped();
        const int published_command = stopped && !previous_stop ? previous_command : command;
        delayed.push_back({tick, 80, measured, published_command, axis.position,
                           axis.velocity, previous_stop});
        previous_stop = stopped;
        previous_command = command;
        axis.step(published_command, true);
    }
    std::ostringstream csv;
    write_trace(csv, delayed);
    std::istringstream input(csv.str());
    const auto issues = verify(read_replay(input));
    ASSERT_FALSE(issues.empty());
    EXPECT_EQ(issues.front().expected_stop, 12);
    EXPECT_EQ(issues.front().observed_stop, 13);
    std::ifstream fixture(FIXTURE_DIR "/delayed_controller.csv");
    ASSERT_TRUE(fixture);
    const std::string saved((std::istreambuf_iterator<char>(fixture)), {});
    EXPECT_EQ(csv.str(), saved);
}

TEST(Replay, StopLatchCannotClearAfterDeadline) {
    std::ifstream fixture(FIXTURE_DIR "/correct_stop.csv");
    auto replay = read_replay(fixture);
    replay.rows.back().stopped = false;
    EXPECT_FALSE(verify(replay).empty());
}

TEST(Control, RejectsUnsupportedSpeedAndScenarioArguments) {
    EXPECT_THROW(Controller(99), std::invalid_argument);
    EXPECT_THROW(Controller(1001), std::invalid_argument);
    EXPECT_THROW(simulate("unknown"), std::invalid_argument);
    EXPECT_THROW(simulate("jam", -1), std::invalid_argument);
    EXPECT_THROW(simulate("jam", 64), std::invalid_argument);
}

TEST(Replay, FaultSweepCoversLastSampleAndTruncatedWindows) {
    for (const auto scenario : {"jam", "frozen"}) {
        for (const int speed : {100, 1000}) {
            for (int fault = 0; fault < 64; ++fault) {
                std::ostringstream csv;
                write_trace(csv, simulate(scenario, fault, 64, speed));
                std::istringstream input(csv.str());
                EXPECT_TRUE(verify(read_replay(input)).empty())
                    << scenario << " fault=" << fault << " speed=" << speed;
            }
        }
    }
    EXPECT_TRUE(simulate("jam", 51)[63].stopped);
    EXPECT_FALSE(simulate("jam", 52)[63].stopped);
}

TEST(Replay, MovementJustBeforeDeadlineRestartsObservationWindow) {
    Replay replay{4, {
        {0, 80, 0, 1, false}, {1, 80, 0, 1, false}, {2, 80, 0, 1, false},
        {3, 80, 1, 1, false}, {4, 80, 1, 1, false}, {5, 80, 1, 1, false},
        {6, 80, 1, 1, false}, {7, 80, 1, 0, true}}};
    EXPECT_TRUE(verify(replay).empty());
}

TEST(Replay, ZeroCommandBreaksQuietInterval) {
    Replay replay{4, {
        {0, 80, 0, 1, false}, {1, 80, 0, 1, false}, {2, 80, 0, 1, false},
        {3, 80, 0, 0, false}, {4, 80, 0, 1, false}, {5, 80, 0, 1, false},
        {6, 80, 0, 1, false}, {7, 80, 0, 1, false}, {8, 80, 0, 0, true}}};
    EXPECT_TRUE(verify(replay).empty());
}

TEST(Replay, ChangingCommandDirectionDoesNotHideMissingFeedback) {
    Replay replay{4, {
        {0, 80, 0, 1, false}, {1, 80, 0, 1, false}, {2, 80, 0, -1, false},
        {3, 80, 0, -1, false}, {4, 80, 0, 0, true}}};
    EXPECT_TRUE(verify(replay).empty());
}

TEST(Replay, ReportsNeverObservedStop) {
    Replay replay{2, {
        {0, 80, 0, 1, false}, {1, 80, 0, 1, false}, {2, 80, 0, 1, false}}};
    const auto issues = verify(replay);
    ASSERT_FALSE(issues.empty());
    EXPECT_EQ(issues.front().tick, 2);
    EXPECT_EQ(issues.front().expected_stop, 2);
    EXPECT_EQ(issues.front().observed_stop, -1);
}

TEST(Replay, ReadsWindowsLineEndingsAndLegacyPolicy) {
    std::ifstream fixture(FIXTURE_DIR "/correct_stop.csv");
    std::string text((std::istreambuf_iterator<char>(fixture)), {});
    size_t position = 0;
    while ((position = text.find('\n', position)) != std::string::npos) {
        text.insert(position, "\r");
        position += 2;
    }
    std::istringstream input(text);
    const auto replay = read_replay(input);
    EXPECT_EQ(replay.quiet_ticks, 4);
    EXPECT_TRUE(verify(replay).empty());
}

TEST(Replay, RejectsInvalidMetadataAndExtraColumns) {
    for (const auto metadata : {
        "# axialtrace,2,step_us=10000,quiet_limit=0",
        "# axialtrace,2,step_us=10000,quiet_limit=-1",
        "# axialtrace,2,step_us=10000,quiet_limit=100001",
        "# axialtrace,3,step_us=10000,quiet_limit=12",
        "# axialtrace,2,step_us=5000,quiet_limit=12"}) {
        std::istringstream input(std::string(metadata) +
            "\ntick,target,encoder,command,position,velocity,stopped\n0,80,0,1,0,0,0\n");
        EXPECT_THROW(read_replay(input), std::runtime_error);
    }
    std::istringstream extra(
        "tick,target,encoder,command,position,velocity,stopped\n0,80,0,1,0,0,0,extra\n");
    EXPECT_THROW(read_replay(extra), std::runtime_error);
}

TEST(Replay, IgnoresPhysicsEvenWhenControllerOutputIsFaulty) {
    auto samples = simulate("jam", 0, 16);
    samples[12].stopped = false;
    samples[12].command = 1000;
    std::ostringstream original;
    write_trace(original, samples);
    std::istringstream first(original.str());
    const auto expected = verify(read_replay(first));
    ASSERT_FALSE(expected.empty());

    for (auto& row : samples) {
        row.position = 999999;
        row.velocity = -999999;
    }
    std::ostringstream changed;
    write_trace(changed, samples);
    std::istringstream second(changed.str());
    const auto actual = verify(read_replay(second));
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(actual[i].tick, expected[i].tick);
        EXPECT_EQ(actual[i].expected_stop, expected[i].expected_stop);
        EXPECT_EQ(actual[i].observed_stop, expected[i].observed_stop);
        EXPECT_EQ(actual[i].reason, expected[i].reason);
    }
}
