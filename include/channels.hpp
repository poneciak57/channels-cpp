
/*
 * Channels-CPP - A high-performance lock-free channel library for C++
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

namespace channels {

/// @brief Overflow strategy for sender when the channel is full
enum class OverflowStrategy {
    /// @brief Block and wait for space (default behavior)
    WAIT_ON_FULL,

    /// @brief Overwrite the oldest unread element
    OVERWRITE_ON_FULL
};

/// @brief Wait strategy for receiver and sender when looping and trying
enum class WaitStrategy {
    /// @brief Busy loop waiting strategy
    /// @note should be used when low latency is required and channel is not expected to wait
    /// @note should be definitly used with OverflowStrategy::OVERWRITE_ON_FULL
    /// @note it uses `asm volatile ("" ::: "memory")` to prevent harmful compiler optimizations
    BUSY_LOOP,

    /// @brief Yielding waiting strategy
    /// @note should be used when low latency is not critical and channel is expected to wait
    /// @note it uses std::this_thread::yield under the hood
    YIELD,

    /// @brief Atomic waiting strategy
    /// @note should be used when low latency is required and channel is expected to wait for longer
    /// @note it uses std::atomic_wait under the hood
    ATOMIC_WAIT,
};

/// @brief Response status for channel operations
enum class ResponseStatus {
    SUCCESS,
    CHANNEL_FULL,
    CHANNEL_EMPTY,

    /// @brief Indicates that the last value is being overwritten or read so try fails
    /// @note This status is only returned if given channel supports OVERWRITE_ON_FULL strategy
    SKIP_DUE_TO_OVERWRITE,

    /// @brief Indicates that the channel is closed
    /// @note closed channel cannot be used and/or opened
    CHANNEL_CLOSED,

    /// @brief Indicates that the receiver is closed
    /// @note closed receiver cannot be used and/or opened
    RECEIVER_CLOSED,

    /// @brief Indicates that the sender is closed
    /// @note closed sender cannot be used and/or opened
    SENDER_CLOSED
};
}