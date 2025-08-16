/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * SPSC Benchmark Tools
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

/// Implementation used in benchmarks

#include <atomic>
#include <memory>
#include <algorithm>

namespace channels::spsc::benchmarks {

template<typename T>
class Sender;
template<typename T>
class Receiver;
template<typename T>
class InnerChannel;

/// @brief Create a bounded single-producer, single-consumer channel
/// @param capacity The minimum capacity of the channel
/// @tparam T The type of values sent through the channel
/// @return A pair of sender and receiver for the channel
template <typename T>
std::pair<Sender<T>, Receiver<T>> channel(size_t capacity) {
    auto channel = std::make_shared<InnerChannel<T>>(capacity);
    return { Sender<T>(channel), Receiver<T>(channel) };
}

/// @brief Sender for a single-producer, single-consumer channel
/// @tparam T The type of values sent through the channel
/// It allows to send values to the channel. It is designed to be used only from one thread at a time.
template <typename T>
class Sender {
    /// Disallows sender creation outside of channel function 
    explicit Sender(std::shared_ptr<InnerChannel<T>> chan) : channel_(chan) {}
public:
    Sender() = delete;
    Sender(const Sender&) = delete;
    Sender& operator=(const Sender&) = delete;
    
    Sender& operator=(Sender&& other) {
        channel_ = std::move(other.channel_);
        return *this;
    } 
    Sender(Sender&& other) : channel_(std::move(other.channel_)) {}

    /// @brief Try to send a value to the channel
    /// @param value The value to send
    /// @return True if the value was sent, false if the channel is full
    bool try_send(const T& value) {
        return channel_->try_send(value);
    }

    /// @brief Try to send a value to the channel
    /// @param value The value to send
    /// @return True if the value was sent, false if the channel is full
    bool try_send(T&& value) {
        return channel_->try_send(std::move(value));
    }

    /// @brief Send a value to the channel
    /// @param value The value to send
    /// @note This function is blocking and will wait until the value is sent.
    void send(const T& value) {
        while (!channel_->try_send(value)) {
            // compiler barrier
            asm volatile ("" ::: "memory");
        }
    }

    /// @brief Send a value to the channel
    /// @param value The value to send
    /// @note This function is blocking and will wait until the value is sent.
    void send(T&& value) {
        while (!channel_->try_send(std::move(value))) {
            // compiler barrier
            asm volatile ("" ::: "memory");
        }
    }

private:
    std::shared_ptr<InnerChannel<T>> channel_;

    friend std::pair<Sender<T>, Receiver<T>> channel<T>(size_t capacity);
};

/// @brief Receiver for a single-producer, single-consumer channel
/// @tparam T The type of values sent through the channel
/// It allows to receive values from the channel. It is designed to be used only from one thread at a time.
template <typename T>
class Receiver {
    /// Disallows receiver creation outside of channel function
    explicit Receiver(std::shared_ptr<InnerChannel<T>> chan) : channel_(chan) {}
public:
    Receiver() = delete;
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    Receiver& operator=(Receiver&& other) {
        channel_ = std::move(other.channel_);
        return *this;
    }
    Receiver(Receiver&& other) : channel_(std::move(other.channel_)) {}

    /// @brief Try to receive a value from the channel
    /// @param value The received value
    /// @return True if a value was received, false if the channel is empty
    bool try_receive(T& value) {
        return channel_->try_receive(value);
    }

    /// @brief Receive a value from the channel
    /// @return The received value
    /// @note This function is blocking and will wait until a value is available.
    T receive() {
        T value;
        while (!channel_->try_receive(value)) {
            // compiler barrier
            asm volatile ("" ::: "memory");
        }
        return value;
    }

private:
    std::shared_ptr<InnerChannel<T>> channel_;

    friend std::pair<Sender<T>, Receiver<T>> channel<T>(size_t capacity);
};

/// @brief Inner channel implementation for the SPSC queue
/// @tparam T The type of values sent through the channel
/// This class is not intended to be used directly by users.
/// @note this class is not thread safe and should be wrapped in std::shared_ptr
template <typename T>
class InnerChannel {
public:
    /// @brief Construct a channel with a given capacity
    /// @param capacity The minimum capacity of the channel, for performance it will be allocated with next power of 2
    /// Uses raw memory allocation so the T type is not required to provide default constructors
    /// alignment is the key for performance it makes sure that objects are properly aligned in memory for faster access
    explicit InnerChannel(size_t capacity) : 
        capacity_(next_power_of_2(capacity)),
        capacity_mask_(capacity_ - 1),
        buffer_(static_cast<T*>(operator new[](capacity_ * sizeof(T), std::align_val_t{alignof(T)}))) {
        
        // Initialize cache values for better performance
        rcvCursorCache_ = 0;
        sendCursorCache_ = 0;
    }
    /// This should not be called if there is existing handle to reader or writer
    ~InnerChannel() {
        size_t sendCursor = sendCursor_.load(std::memory_order_seq_cst);
        size_t rcvCursor = rcvCursor_.load(std::memory_order_seq_cst);

        // Call destructors for all elements in the buffer
        size_t i = rcvCursor;
        while (i != sendCursor) {
            buffer_[i].~T();
            i = next_index(i);
        }

        // Deallocate the buffer
        ::operator delete[](buffer_);
    }

    /// @brief Try to send a value to the channel
    /// @param value The value to send
    /// @return True if the value was sent successfully, false if the channel is full
    /// @note This function is lock-free and wait-free
    bool try_send(const T& value) {
        size_t sendCursor = sendCursor_.load(std::memory_order_relaxed); // only sender thread writes this
        size_t next_sendCursor = next_index(sendCursor);

        if (next_sendCursor == rcvCursorCache_) {
            rcvCursorCache_ = rcvCursor_.load(std::memory_order_acquire);
            if (next_sendCursor == rcvCursorCache_) return false; // Buffer full
        }

        // Construct the new element in place
        new (&buffer_[sendCursor]) T(value);
        
        sendCursor_.store(next_sendCursor, std::memory_order_release);
        return true;
    }

    /// @brief Try to send a value to the channel (move version)
    /// @param value The value to send
    /// @return True if the value was sent successfully, false if the channel is full
    /// @note This function is lock-free and wait-free
    bool try_send(T&& value) {
        size_t sendCursor = sendCursor_.load(std::memory_order_relaxed); // only sender thread writes this
        size_t next_sendCursor = next_index(sendCursor);

        if (next_sendCursor == rcvCursorCache_) {
            rcvCursorCache_ = rcvCursor_.load(std::memory_order_acquire);
            if (next_sendCursor == rcvCursorCache_) return false; // Buffer full
        }

        // Construct the new element in place
        new (&buffer_[sendCursor]) T(std::move(value));
        
        sendCursor_.store(next_sendCursor, std::memory_order_release);
        return true;
    }    
    
    /// @brief Try to receive a value from the channel
    /// @param value The variable to store the received value
    /// @return True if the value was received successfully, false if the channel is empty
    /// @note This function is lock-free and wait-free
    bool try_receive(T& value) {
        size_t rcvCursor = rcvCursor_.load(std::memory_order_relaxed); // only receiver thread reads this

        if (rcvCursor == sendCursorCache_) {
            sendCursorCache_ = sendCursor_.load(std::memory_order_acquire);
            if (rcvCursor == sendCursorCache_) return false; // Buffer empty
        }

        value = std::move(buffer_[rcvCursor]);
        buffer_[rcvCursor].~T(); // Call destructor

        rcvCursor_.store(next_index(rcvCursor), std::memory_order_release);
        return true;
    }

private:

    /// @brief Calculate the next power of 2 greater than or equal to n
    /// @param n The input value
    /// @return The next power of 2
    static constexpr size_t next_power_of_2(size_t n) {
        if (n <= 1) return 1;
        
        // Use bit manipulation for efficiency
        size_t power = 1;
        while (power <= n) {
            power <<= 1;
        }
        return power;
    }

    /// @brief Get the next index in a circular buffer
    /// @param val The current index
    /// @return The next index
    /// @note it might not be used for performance but it is a good reference
    inline size_t next_index(const size_t val) const {
        return (val + 1) & capacity_mask_;
    }

    const size_t capacity_;
    const size_t capacity_mask_; // mask for bitwise next_index
    T* buffer_;

    /// Producer-side data (accessed by sender thread)
    alignas(64) std::atomic<size_t> sendCursor_{0};
    alignas(64) size_t rcvCursorCache_{0}; // reduces cache coherency

    /// Consumer-side data (accessed by receiver thread)  
    alignas(64) std::atomic<size_t> rcvCursor_{0};
    alignas(64) size_t sendCursorCache_{0}; // reduces cache coherency

    friend class Sender<T>;
    friend class Receiver<T>;
    friend std::pair<Sender<T>, Receiver<T>> channel<T>(size_t capacity);
};


} // namespace channels::spsc
