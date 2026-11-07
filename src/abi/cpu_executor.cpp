#include "abi/cpu_executor.hpp"

#include <algorithm>

namespace omm::abi {

CpuExecutor::CpuExecutor(uint32_t threads) : threads_(std::max(1u, threads)) {}

CpuExecutor::~CpuExecutor() {
    std::lock_guard call_lock(call_mutex_);
    {
        std::lock_guard state_lock(state_mutex_);
        stopping_ = true;
    }
    work_ready_.notify_all();
    for (auto &thread : workers_) {
        thread.join();
    }
}

CpuExecutor::Batch::Batch(size_t count, const std::function<void(size_t)> &function)
    : count(count), function(function), errors(count) {}

void CpuExecutor::Batch::execute() {
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
}

void CpuExecutor::worker(uint64_t observed_generation) {
    for (;;) {
        std::unique_lock lock(state_mutex_);
        work_ready_.wait(lock, [&]() { return stopping_ || generation_ != observed_generation; });
        if (stopping_) {
            return;
        }
        observed_generation = generation_;
        Batch *batch = batch_;
        lock.unlock();
        batch->execute();
        lock.lock();
        ++completed_workers_;
        if (completed_workers_ == workers_.size()) {
            work_done_.notify_one();
        }
    }
}

void CpuExecutor::run(size_t count, const std::function<void(size_t)> &function) {
    std::lock_guard call_lock(call_mutex_);
    const size_t participants = std::min(count, static_cast<size_t>(threads_));
    if (participants <= 1) {
        for (size_t index = 0; index < count; ++index) {
            function(index);
        }
        return;
    }
    Batch batch(count, function);
    {
        std::lock_guard lock(state_mutex_);
        // Grow lazily: declaring 65 workers does not create 64 threads for a two-task job.
        workers_.reserve(participants - 1);
        while (workers_.size() < participants - 1) {
            const uint64_t observed = generation_;
            workers_.emplace_back([this, observed]() { worker(observed); });
        }
        completed_workers_ = 0;
        batch_ = &batch;
        ++generation_;
    }
    work_ready_.notify_all();
    batch.execute();
    {
        std::unique_lock lock(state_mutex_);
        work_done_.wait(lock, [&]() { return completed_workers_ == workers_.size(); });
        batch_ = nullptr;
    }
    for (const auto &error : batch.errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
}

} // namespace omm::abi
