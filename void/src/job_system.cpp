#include "job_system.h"
#include "arena.h"

namespace VoidEngine
{
JobSystem* JobSystem::Instance = nullptr;

JobSystem& JobSystem::get()
{
    ENGINE_ASSERT(Instance, "Job system is uninitizlied!");
    return *Instance;
}

void JobSystem::startup(Vallocator &allocator, uint32_t desired_worker_count)
{
    this->allocator = &allocator;
    uint32_t hardware_max_worker_count = std::thread::hardware_concurrency();

    hardware_max_worker_count = hardware_max_worker_count > 1 ? hardware_max_worker_count - 1 : 1;
    hardware_max_worker_count = desired_worker_count > 0 ? std::min(desired_worker_count, hardware_max_worker_count) : hardware_max_worker_count;

    ENGINE_ASSERT(hardware_max_worker_count > 0, "Worker count is 0!");
    hardware_max_worker_count = hardware_max_worker_count;
    hardware_max_worker_count = std::min(MAX_WORKER_THREAD, hardware_max_worker_count);
    for(uint32_t idx = 0; idx < hardware_max_worker_count; ++idx)
    {
        workers[worker_count_++] = std::move(std::thread(&JobSystem::worker_main, this, idx));
    }
    desired_worker_count_ = hardware_max_worker_count;
    t_scratches.init();
}

void JobSystem::shutdown()
{
    running = false;
    wake_cv.notify_all();
    for(uint32_t idx = 0; idx < worker_count_; ++idx)
    {
        std::thread& thread = workers[idx];
        if(thread.joinable())
        {
            thread.join();
        }
    }
    t_scratches.destroy();
}

void JobSystem::schedule(Job job, JobPriority priority) 
{
    {
        std::lock_guard lock(wake_mtx);
        bool push_result = job_pool.try_push(job);
        ENGINE_ASSERT(push_result, "Failed to push job into queue!");
    }
    wake_cv.notify_one();
}

void JobSystem::worker_main(uint32_t idx)
{
    t_scratches.init();
    while(running)
    {
        // std::cout << "This is thread numba " << idx << std::endl;

        std::unique_lock lock(wake_mtx);
        wake_cv.wait(lock, [&]{
                return !job_pool.is_empty() || !running;
            });

        if(running)
        {
            Job job;
            bool pop_result = job_pool.try_pop(job);
            ENGINE_ASSERT(pop_result, "Failed to pop from queue!");

            lock.unlock();
            job.fn_(job.context_, job.ex_data_);
        }
    }
    t_scratches.destroy();
    std::cout << "Thread " << idx << " is destroyed!" << std::endl;
}
} // namespace VoidEngine
