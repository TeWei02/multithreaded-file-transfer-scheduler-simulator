#include "worker_manager.h"

#include <algorithm>
#include <chrono>

namespace simulator {

WorkerManager::WorkerManager(std::size_t workers) {
    workers_.reserve(workers);
    for (std::size_t i = 0; i < workers; ++i) {
        workers_.push_back(std::make_unique<WorkerState>());
    }
    for (std::size_t i = 0; i < workers; ++i) {
        workers_[i]->thread = std::thread(&WorkerManager::RunWorker, this, i);
    }
}

WorkerManager::~WorkerManager() {
    Stop();
}

void WorkerManager::SubmitSlice(int worker_id, int request_id, double start_time, double finish_time) {
    if (worker_id < 0 || static_cast<std::size_t>(worker_id) >= workers_.size()) {
        return;
    }

    WorkerState& state = *workers_[worker_id];
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.queue.push(WorkItem{request_id, start_time, finish_time});
    }
    state.cv.notify_one();
}

void WorkerManager::Stop() {
    if (stopping_.exchange(true)) {
        return;
    }

    for (auto& worker : workers_) {
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->stopped = true;
        }
        worker->cv.notify_one();
    }

    for (auto& worker : workers_) {
        if (worker->thread.joinable()) {
            worker->thread.join();
        }
    }
}

std::size_t WorkerManager::size() const {
    return workers_.size();
}

void WorkerManager::RunWorker(std::size_t worker_index) {
    WorkerState& state = *workers_[worker_index];

    for (;;) {
        WorkItem item;
        {
            std::unique_lock<std::mutex> lock(state.mutex);
            state.cv.wait(lock, [&state] {
                return state.stopped || !state.queue.empty();
            });

            if (state.stopped && state.queue.empty()) {
                break;
            }

            item = state.queue.front();
            state.queue.pop();
        }

        const double duration = std::max(0.0, item.finish_time - item.start_time);
        const int sleep_ms = std::max(1, std::min(5, static_cast<int>(duration * 2.0)));
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        (void)item.request_id;
    }
}

}  // namespace simulator
