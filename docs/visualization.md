# Canyon Exchange — Full System Visualization

This page provides a complete, end-to-end view of how Canyon Exchange processes orders and emits market data.

> GitHub supports Mermaid rendering directly in Markdown. Open this file on GitHub to view the diagrams.

---

## 1) End-to-end component flow

```mermaid
flowchart LR
    C[Client / Gateway] -->|Order| I[Ingress / Parser]
    I --> E[Matching Engine]
    E -->|Limit/Cancel logic| B[(Bid Book)]
    E -->|Limit/Cancel logic| A[(Ask Book)]
    E -->|MarketEvent| R[SPSC Ring Buffer]
    R --> M[Market Data Consumer]
    M --> F[Feed Handler]
    F --> X[Subscribers]

    E -. PMR memory resource .-> P[(Pre-allocated memory pool)]
```

---

## 2) Sequence of a limit order that matches

```mermaid
sequenceDiagram
    participant T as Trader
    participant G as Gateway
    participant E as MatchingEngine
    participant O as OrderBooks(bid/ask)
    participant R as SpscRing
    participant C as Consumer

    T->>G: New limit order
    G->>E: process(order)
    E->>O: Find crossing levels (price-time)
    loop while remaining > 0 and crossing exists
        O-->>E: Best maker order (FIFO)
        E->>E: Compute fill qty
        E->>R: publish(Fill event)
        E->>O: decrement/remove maker
    end
    alt remaining > 0
        E->>O: enqueue remaining as resting order
        E->>R: publish(Ack event)
    end
    E->>R: publish(TopOfBook event)
    C->>R: poll()
    R-->>C: MarketEvent stream
```

---

## 3) Cancel path sequence

```mermaid
sequenceDiagram
    participant T as Trader
    participant E as MatchingEngine
    participant L as order_locator_
    participant O as Book level queue
    participant R as SpscRing

    T->>E: Cancel(order_id)
    E->>L: find(order_id)
    alt found
        E->>O: erase order from queue
        E->>L: erase locator entry
        E->>R: publish(Cancelled)
        E->>R: publish(TopOfBook)
    else missing
        E->>R: publish(Rejected)
    end
```

---

## 4) Runtime threading model

```mermaid
flowchart TB
    subgraph Core0[Core 0]
      P[Producer thread\nIngress + MatchingEngine]
    end

    subgraph Core1[Core 1]
      Q[Consumer thread\nMarketDataFeed poller]
    end

    P -->|SPSC publish| RB[(Ring buffer)]
    RB -->|SPSC poll| Q
```

---

## 5) Data model snapshot

```mermaid
erDiagram
    ORDER {
      uint64 id
      enum side
      uint64 price
      uint32 quantity
      enum type
      uint64 cancel_order_id
      uint64 timestamp_ns
    }

    MARKET_EVENT {
      enum type
      uint64 taker_id
      uint64 maker_id
      uint64 price
      uint32 quantity
      uint64 timestamp_ns
    }

    ORDER ||--o{ MARKET_EVENT : generates
```

---

## How to use this visualization

1. Start with **component flow** to understand system boundaries.
2. Read **limit-order sequence** and **cancel sequence** for control flow.
3. Use **threading model** to reason about low-latency behavior.
4. Cross-reference implementation files:
   - `include/canyon/matching_engine.hpp`
   - `src/matching_engine.cpp`
   - `include/canyon/spsc_ring.hpp`
   - `src/main.cpp`
