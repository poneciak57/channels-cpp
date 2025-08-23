# C++ Channels
Simple and blazingly fast C++ implementation of channels for inter-thread communication. This library provides a simple API for creating and using channels, making it easy to send and receive messages between threads while maintaining high performance and low latency.


# Supported Features

## Single-Producer, Single-Consumer (SPSC)
Lock-free and wait-free communication between a single producer and a single consumer. This is achieved through the use of atomic operations and careful memory management, ensuring that both the sender and receiver can operate without blocking each other. This implementation is designed for high performance and low latency. 

### Usage
It provides easy and safe to use APIs for sending and receiving messages between threads. `channels::spsc::channel` returns 
`std::pair` of sender and receiver. Both of these should not be used or referenced in multiple places.
```cpp
#include <thread>
#include <spsc.hpp>

int main() {
    auto [sender, receiver] = channels::spsc::channel<int>();

    /// We move here to avoid dangling references and race conditions
    std::thread producer([sender = std::move(sender)]() {
        sender.send(i);
    });

    /// We move here to avoid dangling references and race conditions
    std::thread consumer([receiver = std::move(receiver)]() {
        int value = receiver.receive();
    });

    producer.join();
    consumer.join();

    return 0;
}
```

### Performance
It outperforms traditional mutex-based approach as well as Boost's lock-free queues in terms of latency and throughput.
Benchmark results can be found in the [benchmark directory](./benchmark).

## Oneshot Channel
A oneshot channel is a type of channel that can be used to send a single message from a sender to a receiver. Once the message is sent and received, the channel is considered "closed" and cannot be reused. This is useful for scenarios where you only need to send a single message and want to avoid the overhead of maintaining a full-fledged channel, e.g., for simple request-response patterns or some callback mechanisms.

### Usage
To create a oneshot channel, you can use the `channels::oneshot::channel` function, which returns a pair of sender and receiver objects. The sender can be used to send a single message, while the receiver can be used to receive that message.

```cpp
#include <oneshot.hpp>

int main() {
    auto [sender, receiver] = channels::oneshot::channel<int>();

    sender.send(42);

    int value = receiver.receive();

    return 0;
}
```

### Performance
Oneshot channels are designed for low-latency communication and can achieve high throughput in scenarios where a single message needs to be sent and received. However, since they are single-use, they may not be suitable for all use cases. It was designed to maintain very high speed and low memory overhead. Benchmark results can be found in the [benchmark directory](./benchmark).

# Future Work
I plan to work on more advanced features and optimizations for the channel library. If you have any requests or ideas, please feel free to reach out, open an issue or make pull request.

## Will Implement
- Separate strategies for receiver and sender
- Implementation of Multi-Producer, Multi-Consumer (MPMC) channel
- SPMC and MPSC wrappers for MPMC with some optimisations
- ~~Implementation Oneshot channel (single-use channel, inspired by Rust's oneshot channel)~~ (Implemented)

## Might consider
- Implementation of broadcast channel (one-to-many)
- Unbounded version of all channels


# Usage
This library is designed to be easy to use and integrate into your C++ projects. Simply include the header files from `include/` and start using the channel API to send and receive messages between threads.

## Examples
Examples can be found in the [examples directory](./examples).

To run the examples you can use `make` in the root directory.
```
make example/spsc
```