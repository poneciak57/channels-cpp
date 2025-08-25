
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

constexpr size_t cache_line_size = 64; // Assuming a cache line size of 64 bytes

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


/// @brief RAII-style allocation guard similar to libc++'s
/// @tparam __alloc 
/// @note This class provides a way to manage dynamic memory allocation and deallocation
/// @note This class is NOT intended to be used directly by the user
template <typename __alloc>
class __allocation_guard {
    using __allocator_traits = std::allocator_traits<__alloc>;
    using __pointer = typename __allocator_traits::pointer;
    using __size_type = typename __allocator_traits::size_type;
    using __value_type = typename __allocator_traits::value_type;
public:
    __allocation_guard(const __alloc& __a, const __size_type& __s, std::align_val_t __alignment = std::align_val_t{alignof(__value_type)}) 
        : __alloc_(__a), __count_(__s), __alignment_(__alignment), __released_(false) {
            if constexpr (requires { __allocator_traits::allocate(__alloc_, __s, __alignment); }) {
                // If the allocator supports aligned allocation, use it
                __ptr_ = __allocator_traits::allocate(__alloc_, __s, __alignment);
            } else {
                // Otherwise, fall back to regular allocation
                __ptr_ = __allocator_traits::allocate(__alloc_, __s);
            }
        }
    __allocation_guard(const __allocation_guard&) = delete;
    __allocation_guard& operator=(const __allocation_guard&) = delete;

    ~__allocation_guard() {
        if (!__released_) {
            if constexpr (requires { __allocator_traits::deallocate(__alloc_, __ptr_, __count_, __alignment_); }) {
                __allocator_traits::deallocate(__alloc_, __ptr_, __count_, __alignment_);
            } else {
                __allocator_traits::deallocate(__alloc_, __ptr_, __count_);
            }
        }
    }

    inline __pointer get() const noexcept{
        return __ptr_;
    }

    inline __pointer release() noexcept {
        __released_ = true;
        return __ptr_;
    }

    inline __size_type size() const noexcept {
        return __count_;
    }

private:
    __alloc __alloc_;
    __pointer __ptr_;
    const __size_type __count_;
    const std::align_val_t __alignment_;
    bool __released_;
};


}