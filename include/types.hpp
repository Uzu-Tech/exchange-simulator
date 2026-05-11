#pragma once

#include "strong_types.hpp"
#include <initializer_list>
#include <vector>

struct BookOrder {
    TraderId  id;
    Volume volume;
};

class OrderQueue {
public:
    OrderQueue() = default;
    explicit OrderQueue(std::initializer_list<BookOrder> orders) 
        : queue_(orders) {}
    
    void push_back(BookOrder order) {
        queue_.push_back(order);
    }

    void pop_front() {
        if (!empty()) head++;
    }

    const BookOrder& operator[](size_t i) const {
        return queue_[head + i];
    }

    BookOrder& front() { return queue_[head]; }
    const BookOrder& front() const { return queue_[head]; }
    bool empty() const noexcept { return head >= queue_.size(); }
    size_t size() const { return queue_.size() - head; }

private:
    std::vector<BookOrder> queue_;
    size_t head = 0;
};

struct PriceLevel {
    Price price;
    Volume total_volume;
    OrderQueue orders; // FIFO for orders
};

enum class Side {
    BID, ASK
};

enum class OrderType {
    MARKET, LIMIT
};

struct OrderRequest {
    TraderId  id;
    OrderType type;
    Price price;
    Volume volume;
    Side side;

    static OrderRequest limit(TraderId  id, Price price, Volume volume, Side side) {
        return OrderRequest(id, OrderType::LIMIT, price, volume, side);
    }

    static OrderRequest market(TraderId  id, Volume volume, Side side) {
        return OrderRequest(id, OrderType::MARKET, Price{0}, volume, side);
    }

private:
    OrderRequest(TraderId  id, OrderType type, Price price, Volume volume, Side side)
        : id(id), type(type), price(price), volume(volume), side(side) {}
};