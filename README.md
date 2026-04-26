# Canyon Exchange

Canyon Exchange is a C++20 high-frequency trading matching engine prototype with:

- **Price-time priority matching** over bid/ask books.
- **Lock-free SPSC ring** for zero-copy market-data publication.
- **PMR-backed memory pool** to reduce allocator jitter.
- **Benchmark harness** for throughput and p99 latency tracking.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run benchmark

```bash
./build/canyon_benchmark 5000000
```

You can pass a larger message count to stabilize percentile numbers.

## Performance tuning workflow

The project is designed to support iterative tuning toward aggressive targets such as **9.1M msgs/s @ 82us p99**:

1. Enable hugepages (`vm.nr_hugepages`) and map hot pools from hugepages.
2. Pin producer/consumer to isolated cores and keep engine on one NUMA node.
3. Profile with `perf` and remove branch/memory stalls from hot paths.
4. Rebuild with `-O3 -march=native -flto`.

## Tests

```bash
ctest --test-dir build --output-on-failure
```
