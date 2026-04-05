#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace simulator {

class WorkerManager {
   public:
    explicit WorkerManager(std::size_t workers);
    ~WorkerManager();

    WorkerManager(const WorkerManager&) = delete;
    WorkerManager& operator=(const WorkerManager&) = delete;

    void SubmitSlice(int worker_id, int request_id, double start_time, double finish_time);
    void Stop();
    std::size_t size() const;

   private:
    struct WorkItem {
        int request_id;
        double start_time;
        double finish_time;
    };

    struct WorkerState {
        std::mutex mutex;
        std::condition_variable cv;
        std::queue<WorkItem> queue;
        bool stopped = false;
        std::thread thread;
    };

    void RunWorker(std::size_t worker_index);

    std::atomic<bool> stopping_{false};
    std::vector<std::unique_ptr<WorkerState>> workers_;
};

}  // namespace simulator

#endif
