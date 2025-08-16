#include <spsc.hpp>
#include <thread>
#include <iostream>

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

int main() {
    std::cout << "Example: Simple" << std::endl;
    example_simple();

    std::cout << "Example: Overflowable" << std::endl;
    example_overflowable();

    std::cout << "Example: Atomic Wait" << std::endl;
    example_atomicwait();

    std::cout << "Example: Wait Busy" << std::endl;
    example_wait_busy();

    return 0;
}
