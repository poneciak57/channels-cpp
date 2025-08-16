/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * Benchmark Configuration
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

#include <cstddef>
#include <sched.h>
#include <pthread.h>

constexpr size_t QUEUE_CAPACITY = 1024;
constexpr size_t SPEED_TEST_QUANTITY = 1000000;
constexpr size_t AVERAGE_EPOCHS = 15;

#ifdef __APPLE__

// CPU pinning but for macos simply
void pin_thread(size_t __cpu_id) {
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

#else 

void pin_thread(size_t cpu_id) {
    // On Linux, we can use sched_setaffinity to pin the thread to a specific CPU core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);  // Pin to specified CPU
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

#endif



