#pragma once

#include <initializer_list>
#include <span>

#include "config.hpp"
#include "primitives.hpp"

enum class Side { BID, ASK };

struct Trade {
    Price price;
    Volume volume;
    TraderId buyer_id;
    TraderId seller_id;
};

// Order Requests are orders sent to the exchange by makers and traders
enum class OrderType { MARKET, LIMIT };

struct OrderRequest {
    TraderId id;
    OrderType type;
    Price price;
    Volume volume;
    Side side;

    static OrderRequest limit(TraderId id, Price price, Volume volume, Side side) {
        return OrderRequest(id, OrderType::LIMIT, price, volume, side);
    }

    static OrderRequest market(TraderId id, Volume volume, Side side) {
        return OrderRequest(id, OrderType::MARKET, Price{0}, volume, side);
    }

private:
    OrderRequest(TraderId id, OrderType type, Price price, Volume volume, Side side)
        : id(id),
          type(type),
          price(price),
          volume(volume),
          side(side) {}
};

// Book orders are resting orders in the orderbook within a specific price level
struct BookOrder {
    TraderId id;
    Volume volume;
};

// Custom queue implementation using vector for speed over deque
class OrderQueue {
public:
    OrderQueue() = default;
    explicit OrderQueue(std::initializer_list<BookOrder> orders) {
        for (const auto& order: orders) {
            push_back(order);
        }
    }

    void push_back(BookOrder order) {
        hard_assert(size_ < config::MAX_ORDERS_PER_LEVEL, "OrderQueue", "Exceeded max orders per level");
        queue_[size_++] = order;
    }

    void pop_front() {
        hard_assert(!empty(), "OrderQueue", "pop_front on empty queue");
        head++;
    }

    BookOrder& front() { 
        hard_assert(!empty(), "OrderQueue", "front on empty queue");
        return queue_[head]; 
    }

    const BookOrder& front() const { 
        hard_assert(!empty(), "OrderQueue", "front on empty queue");
        return queue_[head]; 
    }

    const BookOrder& operator[](size_t i) const { return queue_[head + i]; }
    bool empty() const noexcept { return head >= size_; }
    size_t size() const { return size_ - head; }

private:
    std::array<BookOrder, config::MAX_ORDERS_PER_LEVEL> queue_;
    size_t head = 0;
    size_t size_ = 0;
};

struct PriceLevel {
    Price price;
    Volume volume;
    OrderQueue orders;  // FIFO for orders
};

struct TradingState {
    Timestamp<config::TIMESTAMP_TICK_SIZE> timestamp;
    std::span<const Trade> prev_trades;
    Position position;
};