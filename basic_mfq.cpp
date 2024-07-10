#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

class TaskScheduler {
public:
    TaskScheduler(size_t numThreads);
    ~TaskScheduler();
    
    void enqueueTask(std::function<void()> task, int priority = 0);
    void resizeThreadPool(size_t numThreads);

private:
    void workerThread();

    std::vector<std::thread> workers;               // Vector to hold worker threads
    std::queue<std::function<void()>> highPriorityTasks; // High priority task queue (level 1)
    std::queue<std::function<void()>> lowPriorityTasks;  // Low priority task queue (level 2)
    std::mutex queueMutex;                          // Mutex to protect task queues
    std::condition_variable condition;              // Condition variable to notify workers
    std::atomic<bool> stop;                         // Atomic flag to stop the scheduler
};

TaskScheduler::TaskScheduler(size_t numThreads) : stop(false) {
    resizeThreadPool(numThreads);
}

TaskScheduler::~TaskScheduler() {
    stop.store(true);                                // Set stop flag to true
    condition.notify_all();                          // Notify all worker threads

    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();                           // Join worker threads
        }
    }
}

void TaskScheduler::enqueueTask(std::function<void()> task, int priority) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (priority > 0) {
            highPriorityTasks.push(task);            // Push task into the high priority queue
        } else {
            lowPriorityTasks.push(task);             // Push task into the low priority queue
        }
    }
    condition.notify_one();                          // Notify one worker thread
}

void TaskScheduler::resizeThreadPool(size_t numThreads) {
    stop.store(true);                                // Stop all existing threads
    condition.notify_all();

    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    stop.store(false);                               // Reset stop flag

    workers.clear();                                 // Clear current workers
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&TaskScheduler::workerThread, this);
    }
}

void TaskScheduler::workerThread() {
    while (!stop) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] {
                return stop.load() || !highPriorityTasks.empty() || !lowPriorityTasks.empty();
            });

            if (stop && highPriorityTasks.empty() && lowPriorityTasks.empty()) {
                return;
            }

            if (!highPriorityTasks.empty()) {
                task = std::move(highPriorityTasks.front());
                highPriorityTasks.pop();             // Pop task from the high priority queue
            } else if (!lowPriorityTasks.empty()) {
                task = std::move(lowPriorityTasks.front());
                lowPriorityTasks.pop();              // Pop task from the low priority queue
            }
        }

        if (task) {
            task();                                  // Execute the task
        }
    }
}

int main() {
    TaskScheduler scheduler(4);                      // Create a scheduler with 4 worker threads

    // Enqueue high priority tasks
    scheduler.enqueueTask([] {
        std::cout << "High priority task on thread " << std::this_thread::get_id() << std::endl;
    }, 1);

    // Enqueue low priority tasks
    scheduler.enqueueTask([] {
        std::cout << "Low priority task on thread " << std::this_thread::get_id() << std::endl;
    }, 0);

    // Enqueue more tasks to see the effect of round-robin scheduling
    for (int i = 0; i < 10; ++i) {
        scheduler.enqueueTask([i] {
            std::cout << "Task " << i << " on thread " << std::this_thread::get_id() << std::endl;
        }, i % 2);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for tasks to complete

    return 0;
}
