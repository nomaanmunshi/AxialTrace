#pragma once

#include <iosfwd>
#include <string>
#include <vector>

struct Observation {
    int tick, target, encoder, command;
    bool stopped;
};

struct Replay {
    int quiet_ticks = 4;
    std::vector<Observation> rows;
};

struct Violation {
    int tick;
    int expected_stop;
    int observed_stop;
    std::string reason;
};

Replay read_replay(std::istream& input);
std::vector<Violation> verify(const Replay& replay);
