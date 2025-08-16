

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
    SKIP_DUE_TO_OVERWRITE
};
}