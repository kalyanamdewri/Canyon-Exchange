#include "canyon/matching_engine.hpp"

#include <algorithm>

namespace canyon {

namespace {
constexpr std::uint64_t kNoPrice = 0;
}

std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

MatchingEngine::MatchingEngine(MarketDataFeed& feed, std::size_t pool_bytes)
    : pool_(pool_bytes),
      resource_(pool_.data(), pool_.size()),
      feed_(feed),
      bids_(&resource_),
      asks_(&resource_),
      order_locator_(&resource_) {}

bool MatchingEngine::process(const Order& order) {
  if (order.type == OrderType::Cancel) {
    return handle_cancel(order);
  }
  return handle_limit(order);
}

std::uint64_t MatchingEngine::best_bid() const {
  if (bids_.empty()) {
    return kNoPrice;
  }
  return bids_.begin()->first;
}

std::uint64_t MatchingEngine::best_ask() const {
  if (asks_.empty()) {
    return kNoPrice;
  }
  return asks_.begin()->first;
}

bool MatchingEngine::handle_limit(const Order& order) {
  std::uint32_t remaining = order.quantity;
  bool ok = (order.side == Side::Buy) ? match_buy(order, remaining) : match_sell(order, remaining);
  if (!ok) {
    return false;
  }

  if (remaining > 0) {
    if (order.side == Side::Buy) {
      auto [it, _] = bids_.try_emplace(order.price);
      it->second.push_back({order.id, order.side, order.price, remaining, order.timestamp_ns});
    } else {
      auto [it, _] = asks_.try_emplace(order.price);
      it->second.push_back({order.id, order.side, order.price, remaining, order.timestamp_ns});
    }
    order_locator_[order.id] = Locator{order.side, order.price};
    if (!emit(EventType::Ack, order.id, 0, order.price, remaining, order.timestamp_ns)) {
      return false;
    }
  }

  on_top_of_book(order.timestamp_ns);
  return true;
}

bool MatchingEngine::handle_cancel(const Order& order) {
  auto loc_it = order_locator_.find(order.cancel_order_id);
  if (loc_it == order_locator_.end()) {
    return emit(EventType::Rejected, order.id, order.cancel_order_id, 0, 0, order.timestamp_ns);
  }

  auto loc = loc_it->second;
  if (loc.side == Side::Buy) {
    auto level_it = bids_.find(loc.price);
    if (level_it == bids_.end()) {
      order_locator_.erase(loc_it);
      return emit(EventType::Rejected, order.id, order.cancel_order_id, 0, 0, order.timestamp_ns);
    }
    auto& queue = level_it->second;
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      if (it->id == order.cancel_order_id) {
        const auto qty = it->quantity;
        queue.erase(it);
        order_locator_.erase(loc_it);
        purge_if_empty(loc.side, loc.price);
        on_top_of_book(order.timestamp_ns);
        return emit(EventType::Cancelled, order.id, order.cancel_order_id, loc.price, qty,
                    order.timestamp_ns);
      }
    }
  } else {
    auto level_it = asks_.find(loc.price);
    if (level_it == asks_.end()) {
      order_locator_.erase(loc_it);
      return emit(EventType::Rejected, order.id, order.cancel_order_id, 0, 0, order.timestamp_ns);
    }
    auto& queue = level_it->second;
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      if (it->id == order.cancel_order_id) {
        const auto qty = it->quantity;
        queue.erase(it);
        order_locator_.erase(loc_it);
        purge_if_empty(loc.side, loc.price);
        on_top_of_book(order.timestamp_ns);
        return emit(EventType::Cancelled, order.id, order.cancel_order_id, loc.price, qty,
                    order.timestamp_ns);
      }
    }
  }

  order_locator_.erase(loc_it);
  return emit(EventType::Rejected, order.id, order.cancel_order_id, 0, 0, order.timestamp_ns);
}

bool MatchingEngine::match_buy(const Order& taker, std::uint32_t& remaining) {
  while (remaining > 0 && !asks_.empty()) {
    auto level_it = asks_.begin();
    if (level_it->first > taker.price) {
      break;
    }

    auto& queue = level_it->second;
    while (remaining > 0 && !queue.empty()) {
      auto& maker = queue.front();
      const auto fill_qty = std::min(remaining, maker.quantity);
      maker.quantity -= fill_qty;
      remaining -= fill_qty;
      if (!emit(EventType::Fill, taker.id, maker.id, maker.price, fill_qty, taker.timestamp_ns)) {
        return false;
      }
      if (maker.quantity == 0) {
        order_locator_.erase(maker.id);
        queue.pop_front();
      }
    }

    if (queue.empty()) {
      asks_.erase(level_it);
    }
  }
  return true;
}

bool MatchingEngine::match_sell(const Order& taker, std::uint32_t& remaining) {
  while (remaining > 0 && !bids_.empty()) {
    auto level_it = bids_.begin();
    if (level_it->first < taker.price) {
      break;
    }

    auto& queue = level_it->second;
    while (remaining > 0 && !queue.empty()) {
      auto& maker = queue.front();
      const auto fill_qty = std::min(remaining, maker.quantity);
      maker.quantity -= fill_qty;
      remaining -= fill_qty;
      if (!emit(EventType::Fill, taker.id, maker.id, maker.price, fill_qty, taker.timestamp_ns)) {
        return false;
      }
      if (maker.quantity == 0) {
        order_locator_.erase(maker.id);
        queue.pop_front();
      }
    }

    if (queue.empty()) {
      bids_.erase(level_it);
    }
  }
  return true;
}

void MatchingEngine::purge_if_empty(Side side, std::uint64_t price) {
  if (side == Side::Buy) {
    auto it = bids_.find(price);
    if (it != bids_.end() && it->second.empty()) {
      bids_.erase(it);
    }
    return;
  }

  auto it = asks_.find(price);
  if (it != asks_.end() && it->second.empty()) {
    asks_.erase(it);
  }
}

bool MatchingEngine::emit(EventType type, std::uint64_t taker_id, std::uint64_t maker_id,
                          std::uint64_t price, std::uint32_t quantity, std::uint64_t ts) {
  return feed_.publish(MarketEvent{type, taker_id, maker_id, price, quantity, ts});
}

void MatchingEngine::on_top_of_book(std::uint64_t ts) {
  emit(EventType::TopOfBook, 0, 0, best_bid(), static_cast<std::uint32_t>(best_ask()), ts);
}

}  // namespace canyon
