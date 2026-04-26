#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <memory>
#include <type_traits>
#include <utility>

namespace canyon {

template <typename T, std::size_t Capacity>
class SpscRing {
  static_assert(Capacity > 1, "Capacity must be > 1");

 public:
  SpscRing() : storage_(std::make_unique<Storage[]>(Capacity)) {}
  ~SpscRing() {
    while (pop()) {
    }
  }

  SpscRing(const SpscRing&) = delete;
  SpscRing& operator=(const SpscRing&) = delete;

  template <typename... Args>
  bool emplace(Args&&... args) {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = increment(head);
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    new (&storage_[head]) T(std::forward<Args>(args)...);
    head_.store(next, std::memory_order_release);
    return true;
  }

  std::optional<T> pop() {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    T* ptr = std::launder(reinterpret_cast<T*>(&storage_[tail]));
    std::optional<T> value{std::move(*ptr)};
    ptr->~T();
    tail_.store(increment(tail), std::memory_order_release);
    return value;
  }

 private:
  using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

  static constexpr std::size_t increment(std::size_t idx) { return (idx + 1) % Capacity; }

  alignas(64) std::unique_ptr<Storage[]> storage_;
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace canyon
