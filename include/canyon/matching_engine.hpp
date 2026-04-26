#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory_resource>
#include <optional>
#include <unordered_map>
#include <vector>

#include "canyon/spsc_ring.hpp"

namespace canyon {

enum class Side : std::uint8_t { Buy, Sell };
enum class OrderType : std::uint8_t { Limit, Cancel };

enum class EventType : std::uint8_t { Ack, Fill, Cancelled, Rejected, TopOfBook };

struct Order {
  std::uint64_t id;
  Side side;
  std::uint64_t price;
  std::uint32_t quantity;
  OrderType type{OrderType::Limit};
  std::uint64_t cancel_order_id{0};
  std::uint64_t timestamp_ns{0};
};

struct MarketEvent {
  EventType type;
  std::uint64_t taker_id;
  std::uint64_t maker_id;
  std::uint64_t price;
  std::uint32_t quantity;
  std::uint64_t timestamp_ns;
};

class MarketDataFeed {
 public:
  static constexpr std::size_t kRingSize = 1 << 20;

  bool publish(const MarketEvent& event) { return events_.emplace(event); }
  std::optional<MarketEvent> poll() { return events_.pop(); }

 private:
  SpscRing<MarketEvent, kRingSize> events_;
};

class MatchingEngine {
 public:
  explicit MatchingEngine(MarketDataFeed& feed, std::size_t pool_bytes = 64 * 1024 * 1024);

  bool process(const Order& order);
  std::uint64_t best_bid() const;
  std::uint64_t best_ask() const;

 private:
  struct LiveOrder {
    std::uint64_t id;
    Side side;
    std::uint64_t price;
    std::uint32_t quantity;
    std::uint64_t timestamp_ns;
  };

  using Queue = std::pmr::deque<LiveOrder>;
  using BidBook = std::pmr::map<std::uint64_t, Queue, std::greater<>>;
  using AskBook = std::pmr::map<std::uint64_t, Queue, std::less<>>;

  struct Locator {
    Side side;
    std::uint64_t price;
  };

  bool handle_limit(const Order& order);
  bool handle_cancel(const Order& order);
  bool match_buy(const Order& taker, std::uint32_t& remaining);
  bool match_sell(const Order& taker, std::uint32_t& remaining);
  void purge_if_empty(Side side, std::uint64_t price);
  bool emit(EventType type, std::uint64_t taker_id, std::uint64_t maker_id, std::uint64_t price,
            std::uint32_t quantity, std::uint64_t ts);
  void on_top_of_book(std::uint64_t ts);

  std::vector<std::byte> pool_;
  std::pmr::monotonic_buffer_resource resource_;
  MarketDataFeed& feed_;
  BidBook bids_;
  AskBook asks_;
  std::pmr::unordered_map<std::uint64_t, Locator> order_locator_;
};

std::uint64_t now_ns();

}  // namespace canyon
