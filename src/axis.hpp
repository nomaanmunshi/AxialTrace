#pragma once

#include <iosfwd>
#include <string_view>
#include <vector>

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
    int update(int target, int measured);
    bool stopped() const { return stopped_; }
private:
    int previous_ = 0;
    int command_ = 0;
    int quiet_ticks_ = 0;
    bool stopped_ = false;
};

struct Sample {
    int tick, target, encoder, command, position, velocity;
    bool stopped;
};

std::vector<Sample> simulate(std::string_view scenario);
void write_trace(std::ostream& out, const std::vector<Sample>& samples);
