# Benchmarks

Here is the list of benchmarked open source solutions and their score.

# SPSC Channel
    
## Methods
To benchmark SPSC Channel, we will use the following methods:

1. **Throughput Measurement**: Measure the number of messages processed per second.
2. **Latency Measurement**: Measure the time taken to process 1000000 messages.

## Test environment
Tests will be performed in different environments

1. **Default**: Nothing special
2. **Pinned**: Threads are pinned to specific CPU cores to minimize the noise.

The result will be an average of 2x15 runs. 15 benchmarking function calls and two runs for each configuration.

## Results
The results of the benchmarks will be presented in a tabular format, comparing the performance of different SPSC Queue implementations under various conditions.

- case 1 - 5 seconds run
- case 2 - 1milion messages run

More means better. (Throughput = ops/sec)

### Test #1
| Implementation | Throughput case 1 (default) | Throughput case 1 (pinned) | Throughput case 2 (default) | Throughput case 2 (pinned) |
|----------------|-----------------------|--------------|--------------|--------------|
| Mutex          |            733_529      |    257_435     |    1_969_995   |    1_901_464   |
| Boost          |          47_586_183     |  49_778_122    |   77_319_394   |   76_695_858   |
| SPSC           |          65_557_247     |  64_332_749    |   106_272_749   |   113_833_255   |

> Device: MacBook Pro M1 16GB (if ram matters) 
> There is no thread pinning here i use some priority thing not sure how it works

### Test #2
| Implementation | Throughput case 1 (default) | Throughput case 1 (pinned) | Throughput case 2 (default) | Throughput case 2 (pinned) |
|----------------|-----------------------|--------------|--------------|--------------|
| Mutex          |         10_122_469    |   27_525_350 |   20_149_022 |   31_704_680 |
| Boost          |         28_293_277    |  119_969_760 |  197_746_021 |  207_497_723 |
| SPSC           |         37_795_893    |  190_931_574 |  343_171_620 |  744_827_583 |

> Device: Lenovo IdeaPad (linux, some ryzen)
> Linux has real thread pinning capabilities.
> [!NOTE] might be outdated

# Oneshot channel

## Method
To test its performance i created simple throughput test. In which two threads sends message one to another with `Sender` as a member of the message. So T1 sends message and sender then T2 uses that sender to send back its message and so on and so on. It guarantes synchronization and lets measure performance.

> I also overwrote global alocator with simple one, because creating a channel requires allocating memory on the heap at the time of benchmarking (maybe will change in future or give an option to choose)

## Results
- case 1, no custom allocator (5 seconds)
- case 2, custom allocator (0.5 second, because i do not free memory on this allocator)

| Case | Total time | Throughput  | Total memory allocated |
|------|------------|-----------------|------------------------|
| 1    | 5.1s       | 4_070_520 ops/s | unknown                |
| 2    | 0.51s      | 4_207_170 ops/s | ~68mb                  |

> Device: MacBook Pro M1 16GB (if ram matters) 

## Rusts tokio oneshot 
I recreated same test using rust tokio oneshot channel. It performed little bit better but you need to remember that tokio is very performant asynchronous runtime. Rust throughput on 5 seconds test was `5_400_417` which is slightly better than this implementation.
But that difference might be attributed to the fact that tokio uses a more efficient memory allocator and has less overhead in terms of context switching scheduling and CPU usage.


## Results summary
It does not outperformed SPSC because of the overhead of this benchmarking case design when each thread can send one message at a time and before sending next one it needs to wait on the previous one to be received. But it is lightweight and especially designed for case when one message should be passed (for example some callback system);