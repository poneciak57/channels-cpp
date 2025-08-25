/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * Oneshot Channel Usage Examples
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

#include <thread>
#include <oneshot.hpp>
#include <iostream>

using namespace channels::oneshot;

void example() {
    auto [sender, receiver] = channel<int>();

    std::thread t1([&]() {
        sender.send(57);
    });

    std::thread t2([&]() {
        int value = receiver.receive();
        std::cout << "Received: " << value << std::endl;
    });

    t1.join();
    t2.join();
}

/// In Most cases you want to use ATOMIC_WAIT strategy or YIELD or use any existing spinlock or something
void waiting_example() {
    auto [sender, receiver] = channel<int, channels::WaitStrategy::ATOMIC_WAIT>();

    std::thread t1([&]() {
        int value = receiver.receive();
        std::cout << "Received: " << value << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));
    sender.send(57);

    t1.join();
}

int main() {
    std::cout << "---- Basic example ----" << std::endl;
    example();

    std::cout << "---- Waiting example ----" << std::endl;
    waiting_example();

    return 0;
}