#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace omm::abi {

// Reuses runtime CPU workers across synchronous frame operations. The caller participates.
// Calls on one executor serialize; task bodies must not recursively enter the same executor.
class CpuExecutor {
public:
    explicit CpuExecutor(uint32_t threads);
    ~CpuExecutor();
    CpuExecutor(const CpuExecutor &) = delete;
    CpuExecutor &operator=(const CpuExecutor &) = delete;

    void run(size_t count, const std::function<void(size_t)> &function);

private:
    struct Batch {
        size_t count;
        const std::function<void(size_t)> &function;
        std::atomic<size_t> next{0};
        std::vector<std::exception_ptr> errors;

        Batch(size_t count, const std::function<void(size_t)> &function);
        void execute();
    };

    void worker(uint64_t observed_generation);

    const uint32_t threads_;
    std::mutex call_mutex_;
    std::mutex state_mutex_;
    std::condition_variable work_ready_;
    std::condition_variable work_done_;
    std::vector<std::thread> workers_;
    uint64_t generation_ = 0;
    size_t completed_workers_ = 0;
    Batch *batch_ = nullptr;
    bool stopping_ = false;
};

} // namespace omm::abi
