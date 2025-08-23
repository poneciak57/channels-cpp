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

    /// @brief Checks if the sender end is closed
    /// @return True if the sender end is closed, false otherwise
    /// @note If sender is closed, it means that value has been already sent
    bool is_closed() const noexcept {
        return channel_->isSentCache_;
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
                channel_->isReady_.wait(false, std::memory_order_acquire);
            }
        }
        return value;
    }

    /// @brief Checks if the receiver end is closed
    /// @return True if the receiver end is closed, false otherwise
    /// @note If receiver is closed, it means that value has been already received
    bool is_closed() const noexcept {
        return channel_->isReceivedCache_;
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
    InnerChannel() : 
        value_(static_cast<T*>(operator new(sizeof(T), std::align_val_t{alignof(T)}))),
        isReady_(false) {}

    /// @brief Sends a value through the channel
    /// @tparam U The type of the value being sent
    /// @param value The value to send
    /// @return The status of the send operation (SUCCESS, SENDER_CLOSED)
    template <typename U>
    ResponseStatus send(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) {
        if (isSentCache_) {
            return ResponseStatus::SENDER_CLOSED; // Already has a value
        }
        new (value_) T(std::forward<U>(value));
        isSentCache_ = true; // only used by sender thread
        isReady_.store(true, std::memory_order_release);
        if constexpr (Wait == WaitStrategy::ATOMIC_WAIT) {
            isReady_.notify_one();
        }
        return ResponseStatus::SUCCESS;
    }

    /// @brief Tries to receive a value from the channel
    /// @tparam U The type of the value being received
    /// @param value The variable to store the received value
    /// @return The status of the receive operation (SUCCESS, RECEIVER_CLOSED, CHANNEL_EMPTY)
    template <typename U>
    ResponseStatus try_receive(U& value) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>) {
        if (isReceivedCache_) {
            return ResponseStatus::RECEIVER_CLOSED; // Already has a value
        }
        if (!isReady_.load(std::memory_order_acquire)) {
            return ResponseStatus::CHANNEL_EMPTY; // No value available
        }
        value = std::move(*value_);
        delete value_;
        value_ = nullptr;
        return ResponseStatus::SUCCESS;
    }

private:
    T *value_;

    /// @brief Cached value for faster sender access
    alignas(64) bool isSentCache_ { false };
    
    /// @brief Cached value for faster receiver access
    alignas(64) bool isReceivedCache_ { false };

    alignas(64) std::atomic<bool> isReady_;

    friend class Sender<T, Wait>;
    friend class Receiver<T, Wait>;
};

} // namespace channels::oneshot
