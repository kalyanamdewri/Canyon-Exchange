#include <cassert>
#include <cstdint>
#include <vector>

#include "canyon/matching_engine.hpp"

using namespace canyon;

int main() {
  {
    MarketDataFeed feed;
    MatchingEngine engine(feed);

    Order buy{.id = 1, .side = Side::Buy, .price = 10000, .quantity = 50, .timestamp_ns = now_ns()};
    Order sell{.id = 2, .side = Side::Sell, .price = 10000, .quantity = 20, .timestamp_ns = now_ns()};

    assert(engine.process(buy));
    assert(engine.process(sell));
    assert(engine.best_bid() == 10000);
    assert(engine.best_ask() == 0);
  }

  {
    MarketDataFeed feed;
    MatchingEngine engine(feed);
    assert(engine.process(Order{.id = 10,
                                .side = Side::Buy,
                                .price = 10100,
                                .quantity = 10,
                                .timestamp_ns = now_ns()}));

    assert(engine.process(Order{.id = 11,
                                .type = OrderType::Cancel,
                                .cancel_order_id = 10,
                                .timestamp_ns = now_ns()}));

    assert(engine.best_bid() == 0);
  }

  return 0;
}
