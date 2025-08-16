/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * Boost SPSC Queue Benchmarks (for comparison)
 * 
 * Copyright (c) 2025 poneciak57
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <boost/lockfree/spsc_queue.hpp>
#include "tools/config.hpp"


void warmup() {
    boost::lockfree::spsc_queue<int> queue(QUEUE_CAPACITY);
    std::thread producer([&queue]() {
        for (int i = 0; i < 10000; ++i) {
            while (!queue.push(i)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    std::thread consumer([&queue]() {
        int value;
        for (int i = 0; i < 10000; ++i) {
            while (!queue.pop(value)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    producer.join();
    consumer.join();
}

long double test_throughput_default(double duration_seconds, bool print_results) {
    boost::lockfree::spsc_queue<int> queue(QUEUE_CAPACITY);

    // Measure throughput
    auto start = std::chrono::high_resolution_clock::now();

    std::atomic<bool> running{true};

    int produced = 0;
    int consumed = 0;

    std::thread producer([&queue, &produced, &running]() {
        while (running.load(std::memory_order_relaxed)) {
            while (!queue.push(produced)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
            produced++;
        }
    });
    std::thread consumer([&queue, &consumed, &running]() {
        int value;
        while (running.load(std::memory_order_relaxed)) {
            while (!queue.pop(value)) {
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
    boost::lockfree::spsc_queue<int> queue(QUEUE_CAPACITY);

    // Measure throughput
    auto start = std::chrono::high_resolution_clock::now();
    std::atomic<bool> running{true};

    int produced = 0;
    int consumed = 0;

    std::thread producer([&queue, &produced, &running]() {
        pin_thread(0);
        while (running.load(std::memory_order_relaxed)) {
            while (!queue.push(produced)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
            produced++;
        }
    });
    std::thread consumer([&queue, &consumed, &running]() {
        pin_thread(1);
        int value;
        while (running.load(std::memory_order_relaxed)) {
            while (!queue.pop(value)) {
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
    boost::lockfree::spsc_queue<int> queue(QUEUE_CAPACITY);

    // Measure throughput
    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&queue]() {
        for (int i = 0; i < SPEED_TEST_QUANTITY; ++i) {
            while (!queue.push(i)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    std::thread consumer([&queue]() {
        for (int i = 0; i < SPEED_TEST_QUANTITY; ++i) {
            int value;
            while (!queue.pop(value)) {
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
    boost::lockfree::spsc_queue<int> queue(QUEUE_CAPACITY);

    // Measure throughput
    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&queue]() {
        pin_thread(0);
        for (int i = 0; i < SPEED_TEST_QUANTITY; ++i) {
            while (!queue.push(i)) {
                // compiler barrier
                asm volatile ("" ::: "memory");
            }
        }
    });
    std::thread consumer([&queue]() {
        pin_thread(1);
        for (int i = 0; i < SPEED_TEST_QUANTITY; ++i) {
            int value;
            while (!queue.pop(value)) {
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

    for (int i = 0; i < AVERAGE_EPOCHS * 10; i++) {
        warmup();
    }

    std::cout << std::fixed << std::setprecision(0);
    std::cout << "Boost benchmarks: \n";
    double throughput = 0.0;
    for (int i = 0; i < AVERAGE_EPOCHS; i++) {
        throughput += test_throughput_default(5.0, false);
    }
    std::cout << "Throughput (default): " << throughput / AVERAGE_EPOCHS << " ops/sec\n";

    throughput = 0.0;
    for (int i = 0; i < AVERAGE_EPOCHS; i++) {
        throughput += test_throughput_pinning(5.0, false);
    }
    std::cout << "Throughput (pinned): " << throughput / AVERAGE_EPOCHS << " ops/sec\n";

    throughput = 0.0;
    for (int i = 0; i < AVERAGE_EPOCHS; i++) {
        throughput += test_latency_default(false);
    }
    std::cout << "Latency (default): " << throughput / AVERAGE_EPOCHS << " ops/sec\n";

    throughput = 0.0;
    for (int i = 0; i < AVERAGE_EPOCHS; i++) {
        throughput += test_latency_pinned(false);
    }
    std::cout << "Latency (pinned): " << throughput / AVERAGE_EPOCHS << " ops/sec\n";

    std::cout.flush();
    return 0;
}