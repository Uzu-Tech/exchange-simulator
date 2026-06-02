#pragma once

namespace settings {
inline constexpr std::size_t INITIAL_ORDER_BOOK_DEPTH = 16;  // Initial number of levels on each side
inline constexpr std::size_t MAX_ORDERS_PER_LEVEL = 8; // Max number of orders in one price level
inline constexpr std::size_t MAX_TRADES_PER_SIDE = 8;
inline constexpr std::size_t WARNING_FILE_SIZE = 5e9; // 5GB file size out limit
inline constexpr std::size_t MAX_NUM_LOGS = 1e9; // Can't go more than 1 billion logs
inline constexpr unsigned int TICK_SIZE{100};
} 