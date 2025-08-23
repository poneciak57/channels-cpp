/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * Oneshot Channel Implementation
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

#pragma once

#include <atomic>
#include <memory>
#include <algorithm>
#include <thread>
#include <type_traits>

#include <channels.hpp>

namespace channels::oneshot {

template<typename T, WaitStrategy Wait>
class Sender;
template<typename T, WaitStrategy Wait>
class Receiver;
template<typename T, WaitStrategy Wait>
class InnerChannel;

/// @brief Creates a one-shot channel
/// @tparam T The type of the value sent through the channel
/// @tparam Wait The wait strategy used by the channel
/// @return A pair of Sender and Receiver for the channel
/// One-shot channels are designed for sending a single value from a sender to a receiver.
template <typename T, WaitStrategy Wait = WaitStrategy::BUSY_LOOP>
std::pair<Sender<T, Wait>, Receiver<T, Wait>> channel() {
    auto channel = std::make_shared<InnerChannel<T, Wait>>();
    return {Sender<T, Wait>(channel), Receiver<T, Wait>(channel)};
}

/// @brief Sender for a one-shot channel
/// @tparam T The type of the value sent through the channel
/// @tparam Wait The wait strategy used by the channel
/// It allows to send values through the channel
template<typename T, WaitStrategy Wait = WaitStrategy::BUSY_LOOP>
class Sender {
    explicit Sender(std::shared_ptr<InnerChannel<T, Wait>> channel) noexcept : channel_(channel) {}
public:
    Sender() = default;
    Sender(const Sender&) = delete;
    Sender& operator=(const Sender&) = delete;

    Sender(Sender&& other) noexcept {
        channel_ = std::move(other.channel_);
        other.channel_ = nullptr;
    }

    Sender& operator=(Sender&& other) noexcept {
        if (this != &other) {
            channel_ = std::move(other.channel_);
            other.channel_ = nullptr;
        }
        return *this;
    }

    /// @brief Sends a value through the channel
    /// @param value The value to send
    /// @return The status of the send operation (SUCCESS, SENDER_CLOSED)
    ResponseStatus send(const T& value) noexcept(std::is_nothrow_constructible_v<T, const T&>) {
        return channel_->send(value);
    }

    /// @brief Sends a value through the channel
    /// @param value The value to send
    /// @return The status of the send operation (SUCCESS, SENDER_CLOSED)
    ResponseStatus send(T&& value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
        return channel_->send(std::move(value));
    }

private:
    std::shared_ptr<InnerChannel<T, Wait>> channel_;
    
    friend std::pair<Sender<T, Wait>, Receiver<T, Wait>> channel<T, Wait>(void);
};

/// @brief Receiver for a one-shot channel
/// @tparam T The type of the value received from the channel
/// @tparam Wait The wait strategy used by the channel
/// It allows to receive values from the channel
template<typename T, WaitStrategy Wait = WaitStrategy::BUSY_LOOP>
class Receiver {
    explicit Receiver(std::shared_ptr<InnerChannel<T, Wait>> channel) noexcept : channel_(channel) {}
public:
    Receiver() = default;
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    Receiver(Receiver&& other) noexcept {
        channel_ = std::move(other.channel_);
        other.channel_ = nullptr;
    }

    Receiver& operator=(Receiver&& other) noexcept {
        if (this != &other) {
            channel_ = std::move(other.channel_);
            other.channel_ = nullptr;
        }
        return *this;
    }

    /// @brief Tries to receive a value from the channel
    /// @param value The variable to store the received value
    /// @return The status of the receive operation (SUCCESS, RECEIVER_CLOSED, CHANNEL_EMPTY)
    ResponseStatus try_receive(T& value) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>) {
        return channel_->try_receive(value);
    }

    /// @brief Receives a value from the channel
    /// @return The received value
    /// @note If the receiver is closed this method will block forever, you should not receive from oneshot channel twice
    T receive() noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>) {
        T value;
        while (channel_->try_receive(value) != ResponseStatus::SUCCESS) {
            if constexpr (Wait == WaitStrategy::YIELD) {
                std::this_thread::yield(); // Yield to allow other threads to run
            } else if constexpr (Wait == WaitStrategy::BUSY_LOOP) {
                asm volatile ("" ::: "memory"); // Busy loop, just spin with compiler barrier
            } else if constexpr (Wait == WaitStrategy::ATOMIC_WAIT) {
                channel_->state_.wait(InnerChannel<T, Wait>::NOT_SENT_MASK, std::memory_order_acquire);
            }
        }
        return value;
    }

private:
    std::shared_ptr<InnerChannel<T, Wait>> channel_;

    friend std::pair<Sender<T, Wait>, Receiver<T, Wait>> channel<T, Wait>(void);
};

/// @brief Inner channel implementation for the one-shot channel
/// @note This class is not intended to be used directly by users
template<typename T, WaitStrategy Wait = WaitStrategy::BUSY_LOOP>
class InnerChannel {
public:
    /// @brief Constructor for internal use only
    /// @note This constructor is public to allow std::make_shared to work, but should not be called directly by users
    explicit InnerChannel() noexcept : state_{ 0 } {}

    /// @brief Destructor
    ~InnerChannel() noexcept {
        if (state_.load(std::memory_order_acquire) == InnerChannel<T, Wait>::SENT_MASK) {
            reinterpret_cast<T*>(value_)->~T();
        }
    }

    /// @brief Sends a value through the channel
    /// @tparam U The type of the value being sent
    /// @param value The value to send
    /// @return The status of the send operation (SUCCESS, SENDER_CLOSED)
    template <typename U>
    ResponseStatus send(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) {
        if (state_.load(std::memory_order_acquire) != InnerChannel<T, Wait>::NOT_SENT_MASK) {
            return ResponseStatus::SENDER_CLOSED; // Already sent
        }

        new (value_) T(std::forward<U>(value));
        state_.store(InnerChannel<T, Wait>::SENT_MASK, std::memory_order_release); // Mark as sent
        if constexpr (Wait == WaitStrategy::ATOMIC_WAIT) {
            state_.notify_one();
        }
        return ResponseStatus::SUCCESS;
    }

    /// @brief Tries to receive a value from the channel
    /// @tparam U The type of the value being received
    /// @param value The variable to store the received value
    /// @return The status of the receive operation (SUCCESS, RECEIVER_CLOSED, CHANNEL_EMPTY)
    template <typename U>
    ResponseStatus try_receive(U& value) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>) {
        size_t state = state_.load(std::memory_order_acquire);
        if (state == InnerChannel<T, Wait>::RECEIVED_MASK) {
            return ResponseStatus::RECEIVER_CLOSED; // Already has a value
        }
        if (state == InnerChannel<T, Wait>::NOT_SENT_MASK) {
            return ResponseStatus::CHANNEL_EMPTY; // No value available
        }
        T* valuePtr = reinterpret_cast<T*>(value_);
        value = std::forward<T>(*valuePtr);
        valuePtr->~T();
        state_.store(InnerChannel<T, Wait>::RECEIVED_MASK, std::memory_order_release);
        return ResponseStatus::SUCCESS;
    }

private:
    alignas(alignof(T)) char value_[sizeof(T)];

    /// @brief Current state of the channel
    std::atomic<size_t> state_{ 0 };

    static constexpr size_t RECEIVED_MASK = 2;
    static constexpr size_t SENT_MASK = 1;
    static constexpr size_t NOT_SENT_MASK = 0;

    friend class Sender<T, Wait>;
    friend class Receiver<T, Wait>;
};

} // namespace channels::oneshot
