#include <iostream>
#include <thread>
#include <mutex>

// Shared resource
int counter = 0;

// Mutex to protect the shared resource
std::mutex mtx;

// Function to be run by multiple threads
void incrementCounter(int id) {
    for (int i = 0; i < 10000; ++i) {
        // Lock the mutex before modifying the shared resource
        std::lock_guard<std::mutex> guard(mtx);
        ++counter;
        // Mutex is automatically released when guard goes out of scope
    }
    std::cout << "Thread " << id << " finished.\n";
}

int main() {
    // Create multiple threads
    std::thread t1(incrementCounter, 1);
    std::thread t2(incrementCounter, 2);

    // Wait for all threads to finish
    t1;
    t2;

    // Output the final value of the counter
    std::cout << "Final counter value: " << counter << std::endl;

    return 0;
}
