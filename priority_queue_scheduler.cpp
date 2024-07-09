#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <ctime>

// Task structure with priority
struct Task {
    std::function<void()> func;
    int priority;

    Task(std::function<void()> f, int p) : func(f), priority(p) {}

    // Comparator for priority
    bool operator<(const Task& other) const {
        return priority < other.priority;
    }
};

class TaskScheduler {
public:
    TaskScheduler(size_t numThreads);
    ~TaskScheduler();

    void enqueueTask(std::function<void()> task, int priority);
    void resizeThreadPool(size_t numThreads);

private:
    void workerThread();

    std::vector<std::thread> workers;                 // Vector to hold worker threads
    std::priority_queue<Task> tasks;                  // Priority queue to hold tasks
    std::mutex queueMutex;                            // Mutex to protect task queue
    std::condition_variable condition;                // Condition variable to notify workers
    std::atomic<bool> stop;                           // Atomic flag to stop the scheduler
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
        tasks.emplace(task, priority);               // Push task into the priority queue
    }
    condition.notify_one();                          // Notify one worker thread
}

void TaskScheduler::resizeThreadPool(size_t numThreads) {
    // Stop all existing threads
    stop.store(true);
    condition.notify_all();

    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    // Reset stop flag
    stop.store(false);

    // Resize the pool and start new threads
    workers.clear();
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&TaskScheduler::workerThread, this);
    }
}

void TaskScheduler::workerThread() {
    while (!stop) {
        Task task([]{}, 0);
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] {
                return stop.load() || !tasks.empty();
            });

            if (stop && tasks.empty()) {
                return;
            }

            task = tasks.top();
            tasks.pop();                              // Pop task from the priority queue
        }

        task.func();                                  // Execute the task
    }
}

int main() {

    time_t start_time, end_time;

    auto begin = std::chrono::high_resolution_clock::now();
    TaskScheduler scheduler(4);                      // Create a scheduler with 4 worker threads

    // Enqueue tasks with different priorities
    scheduler.enqueueTask([] {
        std::cout << "Low priority task on thread " << std::this_thread::get_id() << std::endl;
    }, 1);

    scheduler.enqueueTask([] {
        std::cout << "High priority task on thread " << std::this_thread::get_id() << std::endl;
    }, 10);

    scheduler.enqueueTask([] {
        std::cout << "Medium priority task on thread " << std::this_thread::get_id() << std::endl;
    }, 5);

    std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for tasks to complete

    // Resize the thread pool to 6
    // scheduler.resizeThreadPool(6);

    // Enqueue more tasks to see the effect of resizing
    for (int i = 0; i < 10; ++i) {
        scheduler.enqueueTask([i] {
            std::cout << "Task " << i << " on thread " << std::this_thread::get_id() << std::endl;
        }, i % 3);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for tasks to complete
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);

    printf("Time measured: %.3f seconds.\n", elapsed.count() * 1e-9);

    return 0;
}

