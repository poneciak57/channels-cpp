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