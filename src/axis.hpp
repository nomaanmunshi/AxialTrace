#pragma once

#include <iosfwd>
#include <optional>
#include <string_view>
#include <vector>

inline constexpr int subcounts_per_count = 1000;
inline constexpr int step_us = 10000;
inline constexpr int quiet_limit = 12;

struct Actuator {
    int position = 0;
    int velocity = 0;
    void step(int command, bool jammed);
};

class Encoder {
public:
    int sample(int position, bool frozen);
private:
    int last_ = 0;
};

class Controller {
public:
    explicit Controller(int speed = 1000);
    int update(int target, int measured);
    bool stopped() const { return stopped_; }
private:
    int speed_;
    int previous_ = 0;
    int command_ = 0;
    int quiet_ticks_ = 0;
    bool stopped_ = false;
};

struct Sample {
    int tick, target, encoder, command, position, velocity;
    bool stopped;
};

std::vector<Sample> simulate(std::string_view scenario, int fault_tick = 7, int ticks = 64,
                             std::optional<int> speed = std::nullopt);
void write_trace(std::ostream& out, const std::vector<Sample>& samples);
