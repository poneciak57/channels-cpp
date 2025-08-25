/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * Mutex-based SPSC Benchmark Tools
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

/// Implementation used in benchmarks

#include <mutex>
#include <atomic>

namespace channels::spsc::benchmarks {

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