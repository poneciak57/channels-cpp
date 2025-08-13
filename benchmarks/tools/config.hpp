#include <cstddef>
#include <sched.h>
#include <pthread.h>

constexpr size_t QUEUE_CAPACITY = 1024;
constexpr size_t SPEED_TEST_QUANTITY = 1000000;
constexpr size_t AVERAGE_EPOCHS = 15;

#ifdef __APPLE__

// CPU pinning but for macos simply
void pin_thread(size_t __cpu_id) {
    // On macOS, we can set thread priority and QoS class instead of CPU pinning
    
    // Set high priority for real-time performance
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    
    // Try to set real-time scheduling (requires root privileges)
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        // Fallback: set thread priority within normal scheduling
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
    }
}

#else 

void pin_thread(size_t cpu_id) {
    // On Linux, we can use sched_setaffinity to pin the thread to a specific CPU core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);  // Pin to specified CPU
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

#endif



