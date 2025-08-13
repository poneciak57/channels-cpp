
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sched.h>
#include <spsc_mutex.hpp>

using namespace channels::spsc;

constexpr size_t QUEUE_CAPACITY = 1024;
constexpr size_t SPEED_TEST_QUANTITY = 1000000;

// CPU pinning but for macos simply
void set_thread_priority_macos() {
    // On macOS, we can set thread priority and QoS class instead of CPU pinning
    
    // Set high priority for real-time performance
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    
    // Try to set real-time scheduling (requires root privileges)
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        // Fallback: set thread priority within normal scheduling
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
    }
}

void warmup() {
    spsc_mutex<int> queue(QUEUE_CAPACITY);
    std::thread producer([&queue]() {
        for (int i = 0; i < 10000; ++i) {
            while (!queue.write(i)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    std::thread consumer([&queue]() {
        int value;
        for (int i = 0; i < 10000; ++i) {
            while (!queue.read(value)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    producer.join();
    consumer.join();
}

long double test_throughput_default(double duration_seconds, bool print_results) {
    spsc_mutex<int> queue(QUEUE_CAPACITY);

    // Measure throughput
    auto start = std::chrono::high_resolution_clock::now();

    std::atomic<bool> running{true};

    int produced = 0;
    int consumed = 0;

    std::thread producer([&queue, &produced, &running]() {
        while (running.load(std::memory_order_relaxed)) {
            while (!queue.write(produced)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
            produced++;
        }
    });
    std::thread consumer([&queue, &consumed, &running]() {
        int value;
        while (running.load(std::memory_order_relaxed)) {
            while (!queue.read(value)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
            consumed++;
        }
    });

    /// sleep for specified duration
    std::this_thread::sleep_for(std::chrono::duration<double>(duration_seconds));

    running.store(false, std::memory_order_relaxed);
    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> actual_duration = end - start;
    long double throughput = static_cast<long double>(produced + consumed) / actual_duration.count();
    
    if (print_results) {
        std::cout << "Produced: " << produced << ", Consumed: " << consumed << "\n";
        std::cout << "Duration: " << actual_duration.count() << " seconds\n";
        std::cout << "Throughput (default): " << std::fixed << std::setprecision(0) << throughput << " ops/sec\n";
    }
    
    return throughput;
}

long double test_throughput_pinning(double duration_seconds, bool print_results) {
    spsc_mutex<int> queue(QUEUE_CAPACITY);

    // Measure throughput
    auto start = std::chrono::high_resolution_clock::now();
    std::atomic<bool> running{true};

    int produced = 0;
    int consumed = 0;

    std::thread producer([&queue, &produced, &running]() {
        set_thread_priority_macos();
        while (running.load(std::memory_order_relaxed)) {
            while (!queue.write(produced)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
            produced++;
        }
    });
    std::thread consumer([&queue, &consumed, &running]() {
        set_thread_priority_macos();
        int value;
        while (running.load(std::memory_order_relaxed)) {
            while (!queue.read(value)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
            consumed++;
        }
    });

    /// sleep for specified duration
    std::this_thread::sleep_for(std::chrono::duration<double>(duration_seconds));

    running.store(false, std::memory_order_relaxed);
    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> actual_duration = end - start;
    long double throughput = static_cast<long double>(produced + consumed) / actual_duration.count();
    
    if (print_results) {
        std::cout << "Produced: " << produced << ", Consumed: " << consumed << "\n";
        std::cout << "Duration: " << actual_duration.count() << " seconds\n";
        std::cout << "Throughput (high priority): " << std::fixed << std::setprecision(0) << throughput << " ops/sec\n";
    }
    
    return throughput;
}

long double test_latency_default(bool print_results) {
    spsc_mutex<int> queue(QUEUE_CAPACITY);

    // Measure throughput
    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&queue]() {
        for (int i = 0; i < SPEED_TEST_QUANTITY; ++i) {
            while (!queue.write(i)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    std::thread consumer([&queue]() {
        for (int i = 0; i < SPEED_TEST_QUANTITY; ++i) {
            int value;
            while (!queue.read(value)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    long double throughput = static_cast<long double>(SPEED_TEST_QUANTITY + SPEED_TEST_QUANTITY) / duration.count();
    
    if (print_results) {
        std::cout << "Produced: " << SPEED_TEST_QUANTITY << ", Consumed: " << SPEED_TEST_QUANTITY << "\n";
        std::cout << "Duration: " << duration.count() << " seconds\n";
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec\n";
    }
    
    return throughput;
}

long double test_latency_pinned(bool print_results) {
    spsc_mutex<int> queue(QUEUE_CAPACITY);

    // Measure throughput
    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&queue]() {
        set_thread_priority_macos();
        for (int i = 0; i < SPEED_TEST_QUANTITY; ++i) {
            while (!queue.write(i)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    std::thread consumer([&queue]() {
        set_thread_priority_macos();
        for (int i = 0; i < SPEED_TEST_QUANTITY; ++i) {
            int value;
            while (!queue.read(value)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    long double throughput = static_cast<long double>(SPEED_TEST_QUANTITY + SPEED_TEST_QUANTITY) / duration.count();
    
    if (print_results) {
        std::cout << "Produced: " << SPEED_TEST_QUANTITY << ", Consumed: " << SPEED_TEST_QUANTITY << "\n";
        std::cout << "Duration: " << duration.count() << " seconds\n";
        std::cout << "Throughput (high priority): " << std::fixed << std::setprecision(0) << throughput << " ops/sec\n";
    }
    
    return throughput;
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);

    std::cout << "SPSC (Mutex impl) benchmarks: \n";
    std::cout << "Queue capacity: " << QUEUE_CAPACITY << "\n";
    std::cout << "Warmup: \n";
    warmup();
    std::cout << "\n";

    std::cout << "Running throughput test (default)...\n";
    test_throughput_default(5.0, true);
    std::cout << "\n";

    std::cout << "Running throughput test (pinned)...\n";
    test_throughput_pinning(5.0, true);
    std::cout << "\n";

    std::cout << "Running latency test (default)...\n";
    test_latency_default(true);
    std::cout << "\n";

    std::cout << "Running latency test (pinned)...\n";
    test_latency_pinned(true);
    std::cout << "\n";

    std::cout.flush();
    return 0;
}