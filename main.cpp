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
    
    // Add a new task to the scheduler
    void enqueueTask(std::function<void()> task);

private:
    // Function executed by each worker thread
    void workerThread();

    std::vector<std::thread> workers;                // Vector to hold worker threads
    std::queue<std::function<void()>> tasks;         // Queue to hold tasks
    std::mutex queueMutex;                           // Mutex to protect task queue
    std::condition_variable condition;               // Condition variable to notify workers
    std::atomic<bool> stop;                          // Atomic flag to stop the scheduler
};

TaskScheduler::TaskScheduler(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&TaskScheduler::workerThread, this);
    }
}

TaskScheduler::~TaskScheduler() {
    stop.store(true);                              // Set stop flag to true
    condition.notify_all();                        // Notify all worker threads

    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();                         // Join worker threads
        }
    }
}

void TaskScheduler::enqueueTask(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.push(task);                          // Push task into the queue
    }
    condition.notify_one();                        // Notify one worker thread
}

void TaskScheduler::workerThread() {
    while (!stop) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] {
                return stop.load() || !tasks.empty();
            });

            if (stop && tasks.empty()) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();                          // Pop task from the queue
        }

        task();                                    // Execute the task
    }
}

int main() {
    TaskScheduler scheduler(4);                   // Create a scheduler with 4 worker threads

    // Enqueue some tasks
    for (int i = 0; i < 10; ++i) {
        scheduler.enqueueTask([i] {
            std::cout << "Executing task " << i << " on thread " << std::this_thread::get_id() << std::endl;
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for tasks to complete
    return 0;
}

