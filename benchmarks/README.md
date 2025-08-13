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

### Test #1
| Implementation | Throughput case 1 (default) | Throughput case 1 (pinned) | Throughput case 2 (default) | Throughput case 2 (pinned) |
|----------------|-----------------------|--------------|--------------|--------------|
| Mutex          |            733529     |    257435    |    1969995   |    1901464   |
| Boost          |          47586183     |  49778122    |   77319394   |   76695858   |
| SPSC           |          50846662     |  52201800    |   76898212   |   72571863   |

> Device: MacBook Pro M1 16GB (if ram matters) 
> There is no thread pinning here i use some priority thing not sure how it works

### Test #2
| Implementation | Throughput case 1 (default) | Throughput case 1 (pinned) | Throughput case 2 (default) | Throughput case 2 (pinned) |
|----------------|-----------------------|--------------|--------------|--------------|
| Mutex          |            123456     |      10      |      100     |      200     |
| Boost          |            234567     |      20      |      200     |      400     |
| SPSC           |            345678     |      30      |      300     |      600     |

> Device: Lenovo IdeaPad (linux, some ryzen)
> Linux has real thread pinning capabilities.
