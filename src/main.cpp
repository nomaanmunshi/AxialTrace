#include "axis.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: axialtrace forward|reversal|arrival|limits|jam|frozen\n";
        return 2;
    }
    try {
        write_trace(std::cout, simulate(argv[1]));
        return std::cout ? 0 : 1;
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
