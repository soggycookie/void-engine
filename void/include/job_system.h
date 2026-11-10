#pragma once
#include "fixed_queue.h"
#include "vallocator.h"

namespace VoidEngine
{

constexpr uint32_t MAX_WORKER_THREAD = 16;

enum class JobPriority
{
    CRITICAL,
    HIGH,
    NORMAL,
    LOW,
    IDLE,
    COUNT,
};

struct JobCounter;

struct Job
{
    void (*fn_)(void *context, void *ex_data);
    void *context_;
    void *ex_data_;
    JobCounter *counter_;
};

struct JobCounter
{
    std::atomic<int32_t> dependency_count;
    JobPriority on_zero_priority;
    Job on_zero_job;
};

class JobSystem
{
private:
    static JobSystem *Instance;

    Vallocator *allocator;
    FixedQueue<Job, 2048> job_pool;
    std::thread workers[MAX_WORKER_THREAD];
    std::condition_variable wake_cv;
    std::mutex wake_mtx;
    uint32_t worker_count_;
    uint32_t desired_worker_count_;

    bool running;

public:
    JobSystem() : running(true), allocator(nullptr), desired_worker_count_(0), worker_count_(0)
    {
        ENGINE_ASSERT(!Instance, "Job system is already initialized!");
        Instance = this;
    }

    static JobSystem &get();

    uint32_t get_desired_worker_count() const
    {
        return desired_worker_count_;
    }

    void startup(Vallocator &allocator, uint32_t desired_worker_count);
    void shutdown();

    void schedule(Job job, JobPriority priority);

private:
    void worker_main(uint32_t idx);
};

} // namespace VoidEngine
