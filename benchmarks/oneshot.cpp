/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * Oneshot Channel Benchmarks
 * 
 * Copyright (c) 2025 Kacper Poneta (poneciak57)
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


#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <oneshot.hpp>
#include "tools/config.hpp"

#define USE_CUSTOM_ALLOCATOR 0

using namespace channels::oneshot;


// Simple thread-safe bump allocator
class BumpAllocator {
private:
    static constexpr size_t POOL_SIZE = 1024 * 1024 * 1024; // 1GB
    static char pool_[POOL_SIZE];
    static size_t offset_;
    
public:
    static void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // Align the size to the required alignment
        size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

        size_t old_offset = offset_;
        offset_ += aligned_size;

        if (old_offset + aligned_size > POOL_SIZE) {
            // Pool exhausted - fallback to system allocator or abort
            std::cerr << "BumpAllocator pool exhausted!\n";
            std::abort();
        }
        
        void* ptr = pool_ + old_offset;
        
        // Ensure proper alignment
        if (reinterpret_cast<uintptr_t>(ptr) % alignment != 0) {
            size_t adjust = alignment - (reinterpret_cast<uintptr_t>(ptr) % alignment);
            ptr = static_cast<char*>(ptr) + adjust;
        }
        
        return ptr;
    }
    
    static void reset() {
        offset_ = 0;
    }
    
    static size_t used_bytes() {
        return offset_;
    }
};

// Static member definitions
char BumpAllocator::pool_[BumpAllocator::POOL_SIZE];
size_t BumpAllocator::offset_{0};

#if USE_CUSTOM_ALLOCATOR
// Global allocator overrides
void* operator new(size_t size) {
    return BumpAllocator::allocate(size);
}

void* operator new(size_t size, std::align_val_t alignment) {
    return BumpAllocator::allocate(size, static_cast<size_t>(alignment));
}

void* operator new[](size_t size) {
    return BumpAllocator::allocate(size);
}

void* operator new[](size_t size, std::align_val_t alignment) {
    return BumpAllocator::allocate(size, static_cast<size_t>(alignment));
}

// No-op delete functions (we never free in bump allocator)
void operator delete(void*) noexcept {}
void operator delete(void*, std::align_val_t) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete[](void*, std::align_val_t) noexcept {}
void operator delete(void*, size_t) noexcept {}
void operator delete[](void*, size_t) noexcept {}
#endif

struct TestMsg {
    int value;
    Sender<TestMsg> sender;
};

void test_send_rcv_loop(double duration_seconds) {
    auto [sender, receiver] = channel<TestMsg>();
    std::atomic<bool> running{ true };
    int sentFromT1 = 0;
    int sentFromT2 = 0;
    int receivedInT1 = 0;
    int receivedInT2 = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    std::clock_t cpu_start = std::clock();
    
    std::thread t1([&]() {
        auto t2_sender = std::move(sender);
        while (running.load(std::memory_order_relaxed)) {
            auto [tmp_sender, tmp_receiver] = channel<TestMsg>();
            t2_sender.send(TestMsg{ 57, std::move(tmp_sender) });
            sentFromT1++;
            auto rcvd = tmp_receiver.receive();
            t2_sender = std::move(rcvd.sender);
            receivedInT1++;
        }
    });
    
    std::thread t2([&]() {
        Sender<TestMsg> t2_sender;
        auto rcvd = receiver.receive();
        t2_sender = std::move(rcvd.sender);
        while (running.load(std::memory_order_relaxed)) {
            auto [tmp_sender, tmp_receiver] = channel<TestMsg>();
            t2_sender.send(TestMsg{ 57, std::move(tmp_sender) });
            sentFromT2++;
            auto rcvd = tmp_receiver.receive();
            t2_sender = std::move(rcvd.sender);
            receivedInT2++;
        }
    });
    
    std::this_thread::sleep_for(std::chrono::duration<double>(duration_seconds));
    running.store(false, std::memory_order_relaxed);
    
    std::clock_t cpu_end = std::clock();
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    double cpu_time = static_cast<double>(cpu_end - cpu_start) / CLOCKS_PER_SEC;
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Thread 1 sent: " << sentFromT1 << ", received: " << receivedInT1 << "\n";
    std::cout << "Thread 2 sent: " << sentFromT2 << ", received: " << receivedInT2 << "\n";
    std::cout << "Elapsed time: " << elapsed.count() << " seconds\n";
    std::cout << "CPU time: " << cpu_time << " seconds\n";
    std::cout << "CPU usage: " << (cpu_time / elapsed.count()) * 100 << "%\n";
    std::cout << "Throughput (messages/sec): " << (sentFromT1 + sentFromT2) / elapsed.count() << "\n";

    #if USE_CUSTOM_ALLOCATOR
        std::cout << "Memory used: " << BumpAllocator::used_bytes() << " bytes\n";
    #endif
}

int main() {

    /// Oneshot channel at the time of performing that benchmark is using new operator
    /// It is used almost all the time in this test because we create and delete oneshot channels
    /// So to better test actuall performance of the channel we can run shorter test with very simple and efficient allocation
    #if USE_CUSTOM_ALLOCATOR
        std::cout << "\n=== Testing with bump allocator ===\n";
        test_send_rcv_loop(0.5);
    #else
        std::cout << "\n=== Testing with system allocator ===\n";
        test_send_rcv_loop(5);
    #endif
    return 0;
}