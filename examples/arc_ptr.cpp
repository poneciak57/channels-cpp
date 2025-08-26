/*
 * Channels-CPP - A high-performance lock-free channel library for C++
 * Arc Ptr Channel Usage Examples
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

 #include <iostream>
 #include <thread>
 #include <arc_ptr.hpp>

using namespace channels;

// Arc_ptr works similarly to shared_ptr but is slightly faster
// shared_ptr makes two allocations on creation when arc_ptr does only one
void example() {
    arc_ptr<int> aptr1(1);
    arc_ptr<int> aptr2 = make_arc<int>(2);
    arc_ptr<int> aptr3(aptr1); // copy
    arc_ptr<int> aptr4 = aptr2; // copy
    arc_ptr<int> aptr5 = std::move(aptr1); // move

    // You can safely pass arc_ptr across threads
    std::thread t([aptr2]() mutable {
        std::cout << "Arc_ptr value: " << *aptr2 << "\n";

        // But remember it is not safe to modify its data without any other synchronization mechanism
        // That is why -> and * operators return const T&
        // If you are sure and need to modify the data you can use get_mut but it is not recommended
        *aptr2.get_mut() = 3;
    });
    t.join();
    std::cout << "Arc_ptr value: " << *aptr2 << " (again) \n";
}

int main() {

    example();

    return 0;
 }