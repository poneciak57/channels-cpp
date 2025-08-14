# Benchmarks

Here is the list of benchmarked open source solutions and their score.

# SPSC Queue

## Methods
To benchmark SPSC Queue, we will use the following methods:

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
| SPSC           |          50_846_662     |  52_201_800    |   76_898_212   |   72_571_863   |

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