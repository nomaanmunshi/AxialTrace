#include "replay.hpp"
#include <array>
#include <charconv>
#include <istream>
#include <stdexcept>
#include <string_view>

namespace {
int number(std::string_view field) {
    int value = 0;
    const auto result = std::from_chars(field.data(), field.data() + field.size(), value);
    if (field.empty() || result.ec != std::errc{} || result.ptr != field.data() + field.size())
        throw std::runtime_error("invalid integer in trace");
    return value;
}

bool line(std::istream& input, std::string& text) {
    if (!std::getline(input, text)) return false;
    if (!text.empty() && text.back() == '\r') text.pop_back();
    return true;
}
}

Replay read_replay(std::istream& input) {
    Replay replay;
    std::string text;
    if (!line(input, text)) throw std::runtime_error("empty trace");
    constexpr std::string_view prefix = "# axialtrace,2,step_us=10000,quiet_limit=";
    if (text.compare(0, prefix.size(), prefix) == 0) {
        replay.quiet_ticks = number(std::string_view(text).substr(prefix.size()));
        if (replay.quiet_ticks < 1 || replay.quiet_ticks > 100000)
            throw std::runtime_error("invalid quiet_limit");
        if (!line(input, text)) throw std::runtime_error("missing CSV header");
    }
    if (text != "tick,target,encoder,command,position,velocity,stopped")
        throw std::runtime_error("unsupported trace header");
    while (line(input, text)) {
        std::array<int, 7> fields{};
        std::string_view rest(text);
        for (size_t i = 0; i < fields.size(); ++i) {
            const auto comma = rest.find(',');
            if ((comma == std::string_view::npos) != (i == fields.size() - 1))
                throw std::runtime_error("expected seven CSV columns");
            fields[i] = number(rest.substr(0, comma));
            if (comma != std::string_view::npos) rest.remove_prefix(comma + 1);
        }
        if (fields[0] != static_cast<int>(replay.rows.size()))
            throw std::runtime_error("ticks must start at zero and be contiguous");
        if (fields[1] < 0 || fields[1] > 100 || fields[2] < 0 || fields[2] > 100)
            throw std::runtime_error("target or encoder out of range");
        if (fields[6] != 0 && fields[6] != 1)
            throw std::runtime_error("stopped must be zero or one");
        // Physics columns are parsed for file integrity, then discarded.
        replay.rows.push_back({fields[0], fields[1], fields[2], fields[3], fields[6] == 1});
    }
    if (input.bad()) throw std::runtime_error("trace read failed");
    if (replay.rows.empty()) throw std::runtime_error("trace has no samples");
    return replay;
}

std::vector<Violation> verify(const Replay& replay) {
    if (replay.quiet_ticks < 1 || replay.rows.empty())
        throw std::runtime_error("invalid replay");
    int deadline = -1;
    int observed = -1;
    int last_motion = 0;
    std::vector<Violation> issues;
    for (const auto& row : replay.rows) {
        if (row.stopped && observed == -1) observed = row.tick;
    }
    for (size_t i = 0; i < replay.rows.size(); ++i) {
        const auto& row = replay.rows[i];
        if (row.tick != static_cast<int>(i))
            throw std::runtime_error("noncontiguous replay");
        if (i > 0) {
            const auto& previous = replay.rows[i - 1];
            if (previous.command == 0 || row.encoder != previous.encoder)
                last_motion = row.tick;
            if (deadline == -1 && row.tick - last_motion >= replay.quiet_ticks)
                deadline = row.tick;
        }
        const bool expected = deadline != -1;
        if (row.stopped != expected)
            issues.push_back({row.tick, deadline, observed,
                              expected ? "stop flag missing" : "premature stop flag"});
        if (expected && row.command != 0)
            issues.push_back({row.tick, deadline, observed, "nonzero command after stop deadline"});
    }
    return issues;
}
