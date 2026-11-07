#include "abi/cpu_executor.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <latch>
#include <set>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

std::set<std::thread::id> participants(omm::abi::CpuExecutor &executor) {
    std::set<std::thread::id> threads;
    std::mutex mutex;
    std::latch arrived(4);
    executor.run(4, [&](size_t) {
        {
            std::lock_guard lock(mutex);
            threads.insert(std::this_thread::get_id());
        }
        arrived.count_down();
        arrived.wait();
    });
    return threads;
}

void test_reuse_and_errors() {
    omm::abi::CpuExecutor executor(4);
    executor.run(2, [](size_t) {}); // Start with a smaller batch; grow workers on demand.
    const auto first = participants(executor);
    const auto second = participants(executor);
    expect(first.size() == 4 && first.contains(std::this_thread::get_id()), "caller plus three workers execute concurrently");
    expect(first == second, "successive frame batches reuse the same workers");
    std::atomic<size_t> visited{0};
    bool rejected = false;
    try {
        executor.run(16, [&](size_t index) {
            visited.fetch_add(1);
            if (index == 1) {
                throw std::runtime_error("first");
            }
            if (index == 7) {
                throw std::runtime_error("later");
            }
        });
    } catch (const std::runtime_error &error) {
        rejected = std::string(error.what()) == "first";
    }
    expect(rejected && visited == 16, "all tasks finish and the first input-order failure propagates");
    executor.run(0, [&](size_t) { visited.fetch_add(1); });
    executor.run(1, [&](size_t) { visited.fetch_add(1); });
    expect(visited == 17, "executor remains usable after failure, empty and serial batches");
}

void test_concurrent_callers() {
    omm::abi::CpuExecutor executor(4);
    std::array<std::atomic<int>, 128> visits{};
    auto run = [&](size_t offset) {
        executor.run(64, [&](size_t index) { visits[offset + index].fetch_add(1); });
    };
    std::thread first(run, 0);
    std::thread second(run, 64);
    first.join();
    second.join();
    for (const auto &count : visits) {
        expect(count == 1, "concurrent callers retain independent task state");
    }
}

} // namespace

int main() {
    test_reuse_and_errors();
    test_concurrent_callers();
    if (failures != 0) {
        return 1;
    }
    std::puts("cpu_executor_test: passed");
    return 0;
}
