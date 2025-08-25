/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * SPSC Channel Usage Examples
 * 
 * Copyright (c) 2025 Kacper Poneta (poneciak57)
 * 
 * This implementation is an original work by the author, but it is 
 * inspired by and derived in part from concepts and code published 
 * in the "react-native-audio-api" project by Software Mansion, 
 * licensed under the MIT License.
 * 
 * Original upstream copyright:
 * Copyright (c) 2025 Software Mansion
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

#include <spsc.hpp>
#include <thread>
#include <iostream>

using namespace channels;
using namespace channels::spsc;


void example_simple() {
    auto [sender, receiver] = channel<int>(16);

    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            sender.send(i);
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < 100; ++i) {
            int value = receiver.receive();
            std::cout << "Received: " << value << std::endl;
        }
    });

    producer.join();
    consumer.join();
}

/// THIS IS WRONG !!!
/// this move is executed on another thread imagine that instead of joining
/// we would detach the threads, destructor of sender or receiver leaving the scope of the function
/// can fire before move which would cause undefined behavior or data race
void example_moving_UNSAFE() {
    auto [sender, receiver] = channel<int>(16);

    std::thread producer([&]() {
        /// THIS IS WRONG !!!
        auto s = std::move(sender);
        for (int i = 0; i < 100; ++i) {
            s.send(i);
        }
    });

    std::thread consumer([&]() {
        /// THIS IS WRONG !!!
        auto r = std::move(receiver);
        for (int i = 0; i < 100; ++i) {
            int value = r.receive();
            std::cout << "Received: " << value << std::endl;
        }
    });

    producer.join();
    consumer.join();
}

// SECURE WAY 1: Move by value capture - objects are moved into lambda capture
void example_secure_move_by_value() {
    auto [sender, receiver] = channel<int>(16);

    // Move sender directly into lambda capture
    std::thread producer([sender = std::move(sender)]() mutable {
        for (int i = 0; i < 100; ++i) {
            sender.send(i);
            std::cout << "Sent: " << i << std::endl;
        }
    });

    // Move receiver directly into lambda capture  
    std::thread consumer([receiver = std::move(receiver)]() mutable {
        for (int i = 0; i < 100; ++i) {
            int value = receiver.receive();
            std::cout << "Received: " << value << std::endl;
        }
    });

    producer.join();
    consumer.join();
}

// SECURE WAY 2: Using std::thread constructor with moved objects
void example_secure_move_via_constructor() {
    auto [sender, receiver] = channel<int>(16);

    // Define thread functions that take moved objects as parameters
    auto producer_func = [](Sender<int> s) {
        for (int i = 0; i < 100; ++i) {
            s.send(i);
            std::cout << "Sent: " << i << std::endl;
        }
    };

    auto consumer_func = [](Receiver<int> r) {
        for (int i = 0; i < 100; ++i) {
            int value = r.receive();
            std::cout << "Received: " << value << std::endl;
        }
    };

    // Move objects directly to thread constructor
    std::thread producer(producer_func, std::move(sender));
    std::thread consumer(consumer_func, std::move(receiver));

    producer.join();
    consumer.join();
}

// SECURE WAY 3: Using detached threads with proper lifetime management
void example_secure_detached_threads() {
    auto [sender, receiver] = channel<int>(16);
    
    // Use a synchronization mechanism to ensure threads complete
    std::atomic<int> completed_threads{0};
    
    // Producer thread - moves sender into capture
    std::thread producer([sender = std::move(sender), &completed_threads]() mutable {
        for (int i = 0; i < 100; ++i) {
            sender.send(i);
            std::cout << "Sent: " << i << std::endl;
        }
        completed_threads.fetch_add(1);
    });
    
    // Consumer thread - moves receiver into capture
    std::thread consumer([receiver = std::move(receiver), &completed_threads]() mutable {
        for (int i = 0; i < 100; ++i) {
            int value = receiver.receive();
            std::cout << "Received: " << value << std::endl;
        }
        completed_threads.fetch_add(1);
    });
    
    // Detach threads - they now own their objects
    producer.detach();
    consumer.detach();
    
    // Wait for both threads to complete
    while (completed_threads.load() < 2) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Both detached threads completed" << std::endl;
}

void example_overflowable() {
    auto [sender, receiver] = channel<int, OverflowStrategy::OVERWRITE_ON_FULL>(16);

    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            sender.send(i);
            std::cout << "Sent: " << i << std::endl;
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::thread consumer([&]() {
        int value;
        while (receiver.try_receive(value) != ResponseStatus::CHANNEL_EMPTY) {
            std::cout << "Received: " << value << std::endl;
        }
    });

    producer.join();
    consumer.join();
}

void example_atomicwait() {
    auto [sender, receiver] = channel<int, OverflowStrategy::WAIT_ON_FULL, WaitStrategy::ATOMIC_WAIT>(16);

    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            sender.send(i);
            std::cout << "Sent: " << i << std::endl;
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::thread consumer([&]() {
        for (int i = 0; i < 100; ++i) {
            int value = receiver.receive();
            std::cout << "Received: " << value << std::endl;
        }
    });

    producer.join();
    consumer.join();
}

void example_wait_busy() {
    auto [sender, receiver] = channel<int, OverflowStrategy::WAIT_ON_FULL, WaitStrategy::BUSY_LOOP>(16);

    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            sender.send(i);
            std::cout << "Sent: " << i << std::endl;
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::thread consumer([&]() {
        for (int i = 0; i < 100; ++i) {
            int value = receiver.receive();
            std::cout << "Received: " << value << std::endl;
        }
    });

    producer.join();
    consumer.join();

}

struct TestingStruct {
    int id;
    TestingStruct() : id(0) {
        std::cout << "Default constructed TestingStruct with id: " << id << std::endl;
    }
    TestingStruct(int id) : id(id) {
        std::cout << "Constructed TestingStruct with id: " << id << std::endl;
    }
    ~TestingStruct() {
        std::cout << "Destructed TestingStruct with id: " << id << std::endl;
    }
    TestingStruct(TestingStruct&& other) noexcept : id(other.id) {
        std::cout << "Moved TestingStruct with id: " << id << std::endl;
    }
    TestingStruct& operator=(TestingStruct&& other) noexcept {
        id = other.id;
        std::cout << "Moved assignment TestingStruct with id: " << id << std::endl;
        return *this;
    }
    TestingStruct& operator=(const TestingStruct& other) {
        id = other.id;
        std::cout << "Copy assigned TestingStruct with id: " << id << std::endl;
        return *this;
    }
    TestingStruct(const TestingStruct& other) : id(other.id) {
        std::cout << "Copy constructed TestingStruct with id: " << id << std::endl;
    }
};

// This example shows that move semantics are used, the original object is not copied
void example_testing_struct() {
    auto [sender, receiver] = channel<TestingStruct, OverflowStrategy::WAIT_ON_FULL, WaitStrategy::BUSY_LOOP>(16);

    // here we will just move values
    sender.send(TestingStruct(1));
    sender.send(TestingStruct(2));
    sender.send(TestingStruct(3));

    std::cin.ignore();

    // here we will just move values
    auto v1 = receiver.receive();
    std::cout << "Received: " << v1.id << std::endl;
    auto v2 = receiver.receive();
    std::cout << "Received: " << v2.id << std::endl;
    // third value destructor will be called when sender and receiver go out of scope
    // auto v3 = receiver.receive();
    // std::cout << "Received: " << v3.id << std::endl;
}

int main() {
    std::cout << "Example: Simple" << std::endl;
    example_simple();

    std::cout << "Example: Overflowable" << std::endl;
    example_overflowable();

    std::cout << "Example: Atomic Wait" << std::endl;
    example_atomicwait();

    std::cout << "Example: Wait Busy" << std::endl;
    example_wait_busy();

    std::cout << "Example: Secure Move by Value" << std::endl;
    example_testing_struct();

    return 0;
}
