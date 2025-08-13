#include <atomic>

namespace channels::spsc {

template <typename T>
class channel {

public:
    /// @brief Construct a channel with a given capacity
    /// @param capacity The minimum capacity of the channel, for performance it will be allocated with next power of 2
    /// Uses raw memory allocation so the T type is not required to provide default constructors
    /// alignment is the key for performance it makes sure that objects are properly aligned in memory for faster access
    explicit channel(size_t capacity) : 
        capacity_(next_power_of_2(capacity)),
        capacity_mask_(capacity_ - 1),
        buffer_(static_cast<T*>(operator new[](capacity_ * sizeof(T), std::align_val_t{alignof(T)}))) {
        
        // Initialize cache values for better performance
        rcvCursorCache_ = 0;
        sendCursorCache_ = 0;
    }

    /// This should not be called if there is existing handle to reader or writer
    ~channel() {
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
        size_t next_sendCursor = (sendCursor + 1) & capacity_mask_;

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
        size_t next_sendCursor = (sendCursor + 1) & capacity_mask_;

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

        rcvCursor_.store((rcvCursor + 1) & capacity_mask_, std::memory_order_release);
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
};


} // namespace channels::spsc
