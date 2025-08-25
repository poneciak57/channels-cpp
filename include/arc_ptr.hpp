/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * Arc Ptr Implementation
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

#include <atomic>
#include <memory>
#include <type_traits>

namespace channels {

template <typename T>
struct arc_payload {
    std::atomic<size_t> ref_count{1};
    T data;
};

/// @brief Atomic reference counted smart pointer
/// It is a lightweight alternative to std::shared_ptr with a focus on performance.
/// It keeps T object and its reference count in a single control block.
template<typename T>
class arc_ptr {

public:
    arc_ptr() noexcept : inner(nullptr) {}
    arc_ptr(const T& value) : inner(new arc_payload<T>{1, value}) {}
    arc_ptr(T&& value) : inner(new arc_payload<T>{1, std::move(value)}) {}
    arc_ptr(arc_payload<T>* payload) noexcept : inner(payload) {}

    arc_ptr(const arc_ptr& other) noexcept : inner(other.inner) {
        if (inner) {
            inner->ref_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    arc_ptr& operator=(const arc_ptr& other) noexcept(noexcept(release())) {
        if (this != &other) {
            release();
            inner = other.inner;
            if (inner) {
                inner->ref_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    arc_ptr(arc_ptr&& other) noexcept : inner(other.inner) {
        other.inner = nullptr;
    }

    arc_ptr& operator=(arc_ptr&& other) noexcept(noexcept(release())) {
        if (this != &other) {
            release();
            inner = other.inner;
            other.inner = nullptr;
        }
        return *this;
    }

    arc_ptr& operator=(std::nullptr_t) noexcept(noexcept(release())) {
        release();
        return *this;
    }

    ~arc_ptr() noexcept(noexcept(release())) {
        release();
    }

    inline explicit operator bool() const noexcept {
        return inner != nullptr;
    }

    inline const T* get() const noexcept {
        return &inner->data;
    }

    inline T* get_mut() noexcept {
        return &inner->data;
    }

    inline const T& operator*() const noexcept {
        return inner->data;
    }

    inline const T* operator->() const noexcept {
        return &inner->data;
    }

    inline size_t use_count() const noexcept {
        return inner ? inner->ref_count.load(std::memory_order_relaxed) : 0;
    }

private:
    arc_payload<T> *inner;

    void release() noexcept(std::is_nothrow_destructible_v<T>) {
        if (inner) {
            if (inner->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete inner;
            }
            inner = nullptr;
        }
    }
};

template<typename T, typename... Args>
arc_ptr<T> make_arc(Args&&... args) {
    arc_payload<T>* payload = new arc_payload<T>{1, T(std::forward<Args>(args)...)};
    return arc_ptr<T>(payload);
}

}