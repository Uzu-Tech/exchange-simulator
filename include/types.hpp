#include <cstdint>
#include <deque>

using Price   = uint32_t;
using Volume  = uint32_t;
using OrderId = uint64_t;

struct BookOrder {
    OrderId id;
    Volume volume;
};

struct PriceLevel {
    Price price;
    Volume total_volume;
    std::deque<BookOrder> orders; // FIFO for orders
};

enum class Side {
    BID, ASK
};

enum class OrderType {
    MARKET, LIMIT
};

struct OrderRequest {
    OrderId id;
    OrderType type;
    Price price;
    Volume volume;
    Side side;
};