<div align="center">

# 🏔️ Canyon Exchange

**A C++20 price-time priority matching engine with a zero-copy market-data path**

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-cmake-064F8C?logo=cmake&logoColor=white)
![Status](https://img.shields.io/badge/status-prototype-F59E0B)

</div>

---

## 📌 Table of contents

- [✨ Highlights](#-highlights)
- [🧠 What this is](#-what-this-is)
- [🏗️ Architecture (high level)](#️-architecture-high-level)
- [🚀 Quick start](#-quick-start)
- [📊 Performance notes](#-performance-notes)
- [📁 Repository layout](#-repository-layout)
- [🛣️ Suggested next steps](#️-suggested-next-steps)
- [🤝 Contributing](#-contributing)

---

## ✨ Highlights

- **Price-time priority order matching** (FIFO within each price level).
- **Lock-free SPSC ring buffer** for low-overhead market-data publication.
- **Zero-copy event transport** from engine to feed consumer.
- **PMR-backed containers** to reduce allocator jitter in hot paths.
- **Benchmark harness + tests** for local validation and iteration.

---

## 🧠 What this is

Canyon Exchange is a **research/prototype matching engine** intended for systems/performance exploration.
It models a core exchange function: matching incoming buy/sell orders by **best price first, then oldest order first**.

> This repository is intentionally compact and readable, so contributors can iterate on latency/throughput work quickly.

---

## 🏗️ Architecture (high level)

```text
Order Flow (producer thread)
        │
        ▼
┌────────────────────┐
│   MatchingEngine   │
│  - bid/ask books   │
│  - price-time FIFO │
│  - cancel handling │
└─────────┬──────────┘
          │ emits MarketEvent
          ▼
┌────────────────────┐
│ lock-free SpscRing │  (single producer / single consumer)
└─────────┬──────────┘
          │ poll()
          ▼
┌────────────────────┐
│ MarketData consumer│
└────────────────────┘
```

---

## 🚀 Quick start

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

### Run benchmark

```bash
./build/canyon_benchmark 5000000
```

---

## 📊 Performance notes

Current benchmark output depends on machine topology and runtime tuning.
The benchmark is designed to support tuning toward aggressive goals such as:

- **9.1M msgs/s throughput**
- **82 µs p99 latency**

Recommended workflow:

1. Enable hugepages (`vm.nr_hugepages`) for hot allocations.
2. Pin engine/consumer threads to isolated cores.
3. Keep critical threads/memory on one NUMA node.
4. Profile with `perf` and remove branch + cache misses.
5. Rebuild with full optimizations (`-O3 -march=native -flto`).

---

## 📁 Repository layout

```text
include/canyon/
  matching_engine.hpp   # public API (orders, events, engine, feed)
  spsc_ring.hpp         # lock-free SPSC queue
src/
  matching_engine.cpp   # matching logic, cancel flow, event emission
  main.cpp              # benchmark driver
tests/
  engine_tests.cpp      # smoke tests for matching + cancel behavior
CMakeLists.txt
```

---

## 🛣️ Suggested next steps

- Add binary protocol adapters (ITCH/OUCH-like framing).
- Add deterministic replay input for benchmark reproducibility.
- Separate top-of-book and trade channels.
- Add multi-symbol partitioning + core affinity manager.
- Add percentile histograms and per-stage latency breakdown.

---

## 🤝 Contributing

PRs are welcome. Favor changes that include:

- measurable performance impact,
- deterministic tests,
- and clear profiling evidence.
