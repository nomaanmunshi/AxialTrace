#include "axis.hpp"
#include <algorithm>
#include <ostream>
#include <stdexcept>

void Actuator::step(int command, bool jammed) {
    if (jammed) {
        velocity = 0;
        return;
    }
    velocity += std::clamp(command - velocity, -1, 1);
    position = std::clamp(position + velocity, 0, 100);
    if ((position == 0 && velocity < 0) || (position == 100 && velocity > 0))
        velocity = 0;
}

int Encoder::sample(int position, bool frozen) {
    if (!frozen) last_ = position;
    return last_;
}

int Controller::update(int target, int measured) {
    // A sample reflects the previous command interval.
    quiet_ticks_ = command_ != 0 && measured == previous_ ? quiet_ticks_ + 1 : 0;
    previous_ = measured;
    if (quiet_ticks_ >= 4) stopped_ = true;
    const int error = target - measured;
    command_ = stopped_ ? 0 : std::clamp(error, -1, 1);
    return command_;
}

std::vector<Sample> simulate(std::string_view scenario) {
    if (scenario != "forward" && scenario != "arrival" && scenario != "reversal" &&
        scenario != "limits" && scenario != "jam" && scenario != "frozen")
        throw std::invalid_argument("unknown scenario");
    Actuator axis;
    Controller controller;
    Encoder encoder;
    std::vector<Sample> rows;
    for (int tick = 0; tick < 32; ++tick) {
        int target = 80;
        if (scenario == "arrival") target = 12;
        if (scenario == "reversal" && tick >= 12) target = 0;
        if (scenario == "limits") target = 100;
        if (scenario == "limits" && tick == 0) axis.position = 95;
        const int measured = encoder.sample(axis.position, scenario == "frozen" && tick > 7);
        const int command = controller.update(target, measured);
        rows.push_back({tick, target, measured, command, axis.position, axis.velocity,
                        controller.stopped()});
        axis.step(command, scenario == "jam" && tick >= 7);
    }
    return rows;
}

void write_trace(std::ostream& out, const std::vector<Sample>& samples) {
    out << "tick,target,encoder,command,position,velocity,stopped\n";
    for (const auto& s : samples)
        out << s.tick << ',' << s.target << ',' << s.encoder << ',' << s.command
            << ',' << s.position << ',' << s.velocity << ',' << s.stopped << '\n';
}
