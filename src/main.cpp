#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "canyon/matching_engine.hpp"

using namespace canyon;

int main(int argc, char** argv) {
  std::uint64_t target_messages = 5'000'000;
  if (argc > 1) {
    target_messages = std::strtoull(argv[1], nullptr, 10);
  }

  MarketDataFeed feed;
  MatchingEngine engine(feed);
  std::vector<std::uint64_t> latencies;
  latencies.reserve(target_messages);

  std::atomic<bool> running{true};
  std::thread consumer([&] {
    while (running.load(std::memory_order_relaxed)) {
      while (feed.poll()) {
      }
      std::this_thread::yield();
    }
    while (feed.poll()) {
    }
  });

  std::mt19937_64 rng(7);
  std::uniform_int_distribution<std::uint64_t> price_dist(99'00, 101'00);
  std::uniform_int_distribution<std::uint32_t> qty_dist(1, 100);

  const auto t0 = std::chrono::steady_clock::now();
  for (std::uint64_t i = 1; i <= target_messages; ++i) {
    Order order{.id = i,
                .side = (i & 1) ? Side::Buy : Side::Sell,
                .price = price_dist(rng),
                .quantity = qty_dist(rng),
                .timestamp_ns = now_ns()};

    const auto l0 = std::chrono::steady_clock::now();
    if (!engine.process(order)) {
      std::cerr << "market data ring full, rerun with larger ring or dedicated consumer\n";
      running.store(false, std::memory_order_relaxed);
      consumer.join();
      return 2;
    }
    const auto l1 = std::chrono::steady_clock::now();
    latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(l1 - l0).count());
  }
  const auto t1 = std::chrono::steady_clock::now();

  running.store(false, std::memory_order_relaxed);
  consumer.join();

  std::sort(latencies.begin(), latencies.end());
  const auto p99_idx = static_cast<std::size_t>(latencies.size() * 0.99);
  const auto p99_ns = latencies[std::min(p99_idx, latencies.size() - 1)];

  const auto elapsed_s = std::chrono::duration<double>(t1 - t0).count();
  const double throughput = static_cast<double>(target_messages) / elapsed_s;

  std::cout << "Canyon Exchange benchmark\n";
  std::cout << "messages:     " << target_messages << "\n";
  std::cout << "throughput:   " << std::fixed << std::setprecision(2) << throughput / 1'000'000.0
            << " M msgs/s\n";
  std::cout << "p99 latency:  " << std::fixed << std::setprecision(2)
            << static_cast<double>(p99_ns) / 1000.0 << " us\n";
  std::cout << "target guide: 9.1 M msgs/s @ 82 us p99 (with hugepages/NUMA pinning)\n";

  return 0;
}
