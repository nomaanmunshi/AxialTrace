#include "axis.hpp"
#include <algorithm>
#include <locale>
#include <ostream>
#include <stdexcept>

void Actuator::step(int command, bool jammed) {
    if (jammed) {
        velocity = 0;
        return;
    }
    // Velocity changes by at most one encoder count per tick.
    velocity += std::clamp(command - velocity, -1000, 1000);
    position = std::clamp(position + velocity, 0, 100000);
    if ((position == 0 && velocity < 0) || (position == 100000 && velocity > 0))
        velocity = 0;
}

int Encoder::sample(int position, bool frozen) {
    if (!frozen) last_ = (position + subcounts_per_count / 2) / subcounts_per_count;
    return last_;
}

Controller::Controller(int speed) : speed_(speed) {
    if (speed < 100 || speed > 1000)
        throw std::invalid_argument("speed must be in [100, 1000] subcounts/tick");
}

int Controller::update(int target, int measured) {
    if (target < 0 || target > 100 || measured < 0 || measured > 100)
        throw std::invalid_argument("target and encoder must be in [0, 100]");
    // Feedback at tick k describes the interval commanded at k-1.
    quiet_ticks_ = command_ != 0 && measured == previous_ ? quiet_ticks_ + 1 : 0;
    previous_ = measured;
    if (quiet_ticks_ >= quiet_limit) stopped_ = true;
    const int error = (target - measured) * subcounts_per_count;
    command_ = stopped_ ? 0 : std::clamp(error, -speed_, speed_);
    return command_;
}

std::vector<Sample> simulate(std::string_view scenario, int fault_tick, int ticks) {
    if (scenario != "forward" && scenario != "arrival" && scenario != "reversal" &&
        scenario != "limits" && scenario != "jam" && scenario != "frozen" &&
        scenario != "slow")
        throw std::invalid_argument("unknown scenario");
    if (ticks < 1 || ticks > 100000 || fault_tick < 0 || fault_tick >= ticks)
        throw std::invalid_argument("require 1 <= ticks <= 100000 and 0 <= fault-tick < ticks");
    Actuator axis;
    Controller controller(scenario == "slow" ? 100 : 1000);
    Encoder encoder;
    std::vector<Sample> rows;
    rows.reserve(ticks);
    if (scenario == "limits") axis.position = 95000;
    for (int tick = 0; tick < ticks; ++tick) {
        int target = 80;
        if (scenario == "arrival") target = 12;
        if (scenario == "slow") target = 2;
        if (scenario == "reversal" && tick >= 12) target = 0;
        if (scenario == "limits") target = 100;
        const int measured = encoder.sample(axis.position,
            scenario == "frozen" && tick > fault_tick);
        const int command = controller.update(target, measured);
        rows.push_back({tick, target, measured, command, axis.position, axis.velocity,
                        controller.stopped()});
        axis.step(command, scenario == "jam" && tick >= fault_tick);
    }
    return rows;
}

void write_trace(std::ostream& out, const std::vector<Sample>& samples) {
    out.imbue(std::locale::classic());
    out << "# axialtrace,2,step_us=" << step_us << ",quiet_limit=" << quiet_limit << '\n';
    out << "tick,target,encoder,command,position,velocity,stopped\n";
    for (const auto& s : samples)
        out << s.tick << ',' << s.target << ',' << s.encoder << ',' << s.command
            << ',' << s.position << ',' << s.velocity << ',' << s.stopped << '\n';
}
