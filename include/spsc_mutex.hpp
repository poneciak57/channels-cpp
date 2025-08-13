#include <mutex>
#include <atomic>

namespace channels::spsc {

/// @brief A single-producer single-consumer implementation using mutex for locking
/// @tparam T 
template <typename T>
class spsc_mutex {
public:
    spsc_mutex(size_t capacity) : capacity(capacity) {
        buffer = new T[capacity];
    }

    ~spsc_mutex() {
        delete[] buffer;
    }

    bool write(T value) {
        if (mtx.try_lock()) {
            if (size == capacity) {
                mtx.unlock();
                return false;
            }
            // Write the value to the buffer
            buffer[write_index] = value;
            write_index = (write_index + 1) % capacity;
            size++;
            mtx.unlock();
            return true;
        }
        return false;
    }

    bool read(T& value) {
        if (mtx.try_lock()) {
            if (size > 0) {
                // Read the value from the buffer
                value = buffer[read_index];
                read_index = (read_index + 1) % capacity;
                size--;
                mtx.unlock();
                return true;
            }
            mtx.unlock();
        }
        return false;
    }

private:
    T* buffer;
    size_t capacity;
    size_t size{0};
    std::mutex mtx;
    size_t write_index{0};
    size_t read_index{0};
};

}