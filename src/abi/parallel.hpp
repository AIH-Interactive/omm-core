// Bounded synchronous CPU work. Each index owns its output; callers merge in index order.
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <thread>
#include <vector>

namespace omm::abi {

template <typename Function>
void parallel_for(size_t count, uint32_t thread_count, Function &&function) {
    const size_t participants = std::min(count, static_cast<size_t>(std::max(1u, thread_count)));
    if (participants <= 1) {
        for (size_t index = 0; index < count; ++index) {
            function(index);
        }
        return;
    }

    std::atomic<size_t> next{0};
    std::vector<std::exception_ptr> errors(count);
    auto work = [&]() {
        for (;;) {
            const size_t index = next.fetch_add(1, std::memory_order_relaxed);
            if (index >= count) {
                return;
            }
            try {
                function(index);
            } catch (...) {
                errors[index] = std::current_exception();
            }
        }
    };

    // jthread also joins if thread creation fails; borrowed inputs outlive every worker.
    std::vector<std::jthread> workers;
    workers.reserve(participants - 1);
    for (size_t index = 1; index < participants; ++index) {
        workers.emplace_back(work);
    }
    work();
    for (auto &worker : workers) {
        worker.join();
    }
    // Report the first input-order error, independent of scheduling.
    for (const auto &error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
}

} // namespace omm::abi
