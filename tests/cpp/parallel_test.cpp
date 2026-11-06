#include "abi/parallel.hpp"

#include <atomic>
#include <cstdio>
#include <latch>
#include <stdexcept>
#include <string>

int main() {
    // All four tasks must run at once; a serial implementation cannot pass this barrier.
    std::latch ready(4);
    std::atomic<unsigned int> completed{0};
    omm::abi::parallel_for(4, 4, [&](size_t) {
        ready.arrive_and_wait();
        completed.fetch_add(1);
    });
    if (completed != 4) {
        return 1;
    }

    // Failures must join every worker and surface the first input-order exception.
    completed = 0;
    try {
        omm::abi::parallel_for(32, 4, [&](size_t index) {
            completed.fetch_add(1);
            if (index == 3 || index == 17) {
                throw std::runtime_error(std::to_string(index));
            }
        });
        return 1;
    } catch (const std::runtime_error &error) {
        if (std::string(error.what()) != "3" || completed != 32) {
            return 1;
        }
    }
    std::printf("parallel_test: concurrent execution, deterministic errors and joins passed\n");
    return 0;
}
