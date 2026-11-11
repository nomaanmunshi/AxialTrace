#include "axis.hpp"
#include "replay.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
int integer(std::string_view value) {
    int result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::invalid_argument("option requires an integer");
    return result;
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2)
            throw std::invalid_argument(
                "usage: axialtrace SCENARIO [--fault-tick N] [--ticks N] [--speed N] | verify TRACE.csv");
        if (std::string_view(argv[1]) == "verify") {
            if (argc != 3) throw std::invalid_argument("usage: axialtrace verify TRACE.csv");
            std::ifstream input(argv[2]);
            if (!input) throw std::runtime_error("cannot open trace");
            const auto replay = read_replay(input);
            const auto issues = verify(replay);
            for (const auto& issue : issues)
                std::cout << "tick=" << issue.tick << " expected_stop=" << issue.expected_stop
                          << " observed_stop=" << issue.observed_stop << " " << issue.reason << '\n';
            if (issues.empty()) std::cout << "PASS " << replay.rows.size() << " recorded ticks (checked window only)\n";
            return issues.empty() ? 0 : 1;
        }
        int fault_tick = 7;
        int ticks = 64;
        std::optional<int> speed;
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) throw std::invalid_argument("missing option value");
            const std::string_view option(argv[i]);
            if (option == "--fault-tick") fault_tick = integer(argv[i + 1]);
            else if (option == "--ticks") ticks = integer(argv[i + 1]);
            else if (option == "--speed") speed = integer(argv[i + 1]);
            else throw std::invalid_argument("unknown option");
        }
        write_trace(std::cout, simulate(argv[1], fault_tick, ticks, speed));
        return std::cout ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
