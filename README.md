# Exchange Simulator

A high-performance market microstructure simulator built in modern C++20. Implements a complete electronic exchange with matching engine, order book management, pluggable agent-based market participants, and stochastic price models.

## Project Overview

Exchange Simulator is a complete electronic market system written from scratch in C++. The core contribution is demonstrating how to build a highly extensible, high-performance system where any market participant type (maker, taker, strategy) can be mixed with any other without code changes—all compiled to a single, specialized binary with zero runtime polymorphism overhead.

## Core Architecture

### Order Matching Engine

- **Price-Time Priority Matching**: Implements standard exchange rules with FIFO order queues at each price level
- **Order Crossing Logic**: Intelligent bid-ask crossing detection (BID ≥ ASK for matches)
- **Partial Fill Handling**: Supports both complete and partial order fills with proper settlement
- **Market & Limit Orders**: Full support for market orders (execute or nothing) and limit orders (rest on book)
- **FIFO Order Queue**: Custom fixed-size array-based queue with O(1) operations—no dynamic allocation in hot paths
- **Order Lifecycle**: Complete tracking from submission through fill to settlement

### Order Book Management

Efficient depth tracking with automatic cleanup of empty price levels. Fast best-bid/best-ask queries and proper handling of partial fills. The book is rebuilt and snapshotted for metrics collection.

### Simulation Core Loop

```cpp
for each tick:
  1. Update metrics from previous tick state
  2. Fill book from Maker agent pool
  3. Route Strategy orders through matching engine
  4. Process Taker agent market orders
  5. Collect tick metrics and log state
  6. Clear processed orders, advance price, increment tick
```

**Position Management**: Enforces strict risk controls via `PositionLimit`, clamping strategy order volumes to remaining long/short capacity before matching.

### Pluggable Market Participants

The system is built on the principle that **any participant type can be mixed with any other** without code changes. This is achieved through template-based composition and concept constraints.

#### Maker Bots

Market makers implement a configurable interface that generates limit orders to provide liquidity:

```cpp
template<typename T, typename P>
concept MarketMaker = requires(T maker, OrderBook& book, P price) {
    { maker.get_orders(book, price) } -> /* OrderRequest vector */;
};
```

The framework ships with configurable maker implementations, but the key design point is that **new maker strategies can be added by implementing this concept**. Mix multiple different maker types in the same simulation.

#### Taker Bots

Market takers implement market order submission logic, creating realistic order flow:

```cpp
template<typename T, typename P>
concept MarketTaker = requires(T taker, OrderBook& book, P price) {
    { taker.get_order(book, price) } -> std::optional<OrderRequest>;
};
```

Takers can be probabilistic (trade with random chance), reactive (depend on spread/depth), or deterministic. Any combination can run simultaneously. Each taker type is independent and testable.

#### User Strategy

The user's trading algorithm reads the live order book and submits its own orders:

```cpp
struct UserStrategy {
    std::optional<OrderRequest> get_order(
        const OrderBook& book,
        const PriceModel& model,
        Position pos,
        PositionLimit limit
    ) {
        // Read order book, decision logic, return order or nothing
    }
};
```

The strategy sees the full state (best bid/ask, depth, own position) and must respect position limits. It competes with makers and takers for fills.

### Stochastic Price Models

Price models are fully pluggable via concept:

```cpp
template<typename P>
concept PriceModel = requires(P p) {
    { p.next_price() } -> std::convertible_to<double>;
    { p.current_price() } -> std::convertible_to<double>;
};
```

**Design Pattern**: The system lets you define any number of price model implementations, then select which one to use via a compile-time type alias:

```cpp
// active_types.hpp
using ActivePriceModel = SimpleRandomWalk;  // or GaussianRandomWalk, or custom
```

Multiple models can be implemented (random walk, mean-reverting, jump diffusion, etc.), and the build system compiles only the selected one. Zero runtime overhead for the interface—it's pure template specialization.

### Composition: Mix & Match at Compile Time

The true power lies in composition:

```cpp
using ActiveMakers = TypeList<MakerTypeA, MakerTypeB, MakerTypeA>;
using ActiveTakers = TypeList<TakerTypeA, TakerTypeB>;
using ActiveStrategy = MyStrategy;
using ActivePriceModel = GaussianRandomWalk;
```

You can combine:
- Any price model with any strategy
- Multiple different maker types in the same simulation
- Multiple different taker types
- All of the above simultaneously

Each combination compiles into a single, type-safe binary with **zero runtime polymorphism overhead**. No virtual functions, no branching on agent type—the compiler generates specialized code for each type.

### Configuration & Wiring

YAML configuration declares the market structure (how many of each agent type, their parameters). The `SimBuilder` pattern reads the config and constructs a fully wired `Simulator<ActiveMakers, ActiveTakers, ActiveStrategy, ActivePriceModel>`:

```cpp
SimBuilder::build_sim(config_yaml, num_ticks, seed, position_limit)
    → Simulator fully initialized
```

Runtime parameter overrides are supported via CLI (`--set maker.0.half_spread=10`), allowing experiments without recompilation.

## Performance Architecture

### Design Choices for Speed

**No Dynamic Allocation in Hot Loops**

The matching engine and simulation tick loop use only stack allocation and fixed-size arrays:

```cpp
template<size_t MAX_CAPACITY>
class OrderQueue {
    std::array<Order, MAX_CAPACITY> orders;  // Fixed size, no heap
    size_t head = 0, tail = 0;
    
    // O(1) push/pop, zero allocation
};
```

Every order queue is pre-allocated. Order placement never triggers dynamic memory allocation—critical for microsecond-level consistency. Fixed capacities can be tuned in `include/core/settings.hpp`:

```cpp
namespace settings {
    inline constexpr std::size_t MAX_ORDERS_PER_LEVEL = 8;  // Adjust as needed
    inline constexpr std::size_t MAX_TRADES_PER_SIDE = 8;   // Adjust for your workload
};
```

**Template-Based Specialization Reduces Branching**

Instead of runtime type checking (e.g., `if (maker_type == Type::A)`), the compiler generates separate code paths for each maker/taker type:

```cpp
// No if-statements at runtime—compiler unrolls this:
for each maker in TypeList<MakerA, MakerB> {
    maker.get_orders(...);  // Type-specialized, inlinable
}
```

This eliminates branch prediction overhead and enables aggressive inlining.

**High-Performance RNG**

Standard library RNGs (`std::uniform_real_distribution`, `std::normal_distribution`) are slow. This project implements:

- **PCG32**: Fast, high-quality PRNG (2-3x faster than MT19937)
- **Fast Uniform**: Direct bit manipulation instead of floating-point division
- **Fast Normal**: Ziggurat algorithm instead of Box-Muller

These custom implementations are integrated directly into agents and price models, eliminating wrapper overhead.

**Parallel Monte Carlo via `for_each`**

Monte Carlo runs (1000+) are embarrassingly parallel—each run is independent. The simulator uses `std::execution::par` with `for_each` for automatic work distribution across all cores:

```cpp
std::for_each(std::execution::par, runs.begin(), runs.end(),
    [&](int run_idx) {
        auto result = single_run(run_idx);  // Each core gets independent run
    });
```

Single-core runs dominate when num_runs < num_cores. Each core runs its own independent simulation without coordination.

### High-Performance Logging: Memory-Mapped Binary NumPy Files

The traditional approach to logging—buffering data in memory, serializing to text, flushing on disk—is fundamentally at odds with microsecond latency. Instead, this simulator uses **memory-mapped binary files** on Linux to achieve nanosecond logging latency.

#### How It Works

1. **Pre-allocate file**: Use `ftruncate()` to allocate the exact number of bytes needed (header + data)
2. **Memory map**: Use `mmap(MAP_SHARED)` to create a view of the file in virtual memory
3. **Direct write**: Write data directly to mapped memory—the kernel handles persistence
4. **Async flush**: Use `madvise(MADV_SEQUENTIAL)` for cache hints and `msync(MS_ASYNC)` for async writeback
5. **Shrink on exit**: Truncate to actual size written

The result: **single write operation is a single memory copy** (~10ns), with kernel handling buffering and disk I/O asynchronously.

#### Code Example

```cpp
template<typename Log>
class MappedDatasetFile {
public:
    MappedDatasetFile(const fs::path& file_path, size_t max_logs)
        : file_desc(file_path), max_logs(max_logs) {
        // Pre-allocate file to exact size
        size_t num_bytes = sizeof(Log) * max_logs + header_byte_size;
        ftruncate(file_desc.value(), num_bytes);

        // Create shared memory mapping
        void* ptr = mmap(nullptr, num_bytes, PROT_READ | PROT_WRITE, 
                         MAP_SHARED, file_desc.value(), 0);
        
        // Hint kernel for sequential access
        madvise(ptr, num_bytes, MADV_SEQUENTIAL);
        
        data = std::span<Log>(reinterpret_cast<Log*>(ptr) + header_offset, max_logs);
    }

    // Write is just a memory copy
    void write(Log log) {
        data[num_logs++] = log;  // O(1), no syscalls
    }

    ~MappedDatasetFile() {
        // Flush kernel buffers and unmap
        msync(mapped_ptr, mapped_byte_size, MS_ASYNC);
        munmap(mapped_ptr, mapped_byte_size);
        
        // Trim to actual size
        ftruncate(file_desc.value(), actual_bytes);
    }
};
```

#### Log Schema with NumPy Headers

Data is stored in NumPy structured array format—binary data with a descriptive header:

```cpp
struct PerformanceLog {
    Tick tick;
    double pnl;
    Position position;

    static constexpr auto npy_fields() {
        return std::make_tuple(
            std::make_pair("tick", NpyType<Tick::Underlying>::code),
            std::make_pair("pnl", NpyType<double>::code),
            std::make_pair("position", NpyType<Position::Underlying>::code)
        );
    }
};

struct TradeLog {
    Tick tick;
    TraderId buyer_id;
    TraderId seller_id;
    Price price;
    Volume volume;

    static constexpr auto npy_fields() {
        return std::make_tuple(
            std::make_pair("tick", NpyType<Tick::Underlying>::code),
            std::make_pair("buyer_id", NpyType<TraderId::Underlying>::code),
            std::make_pair("seller_id", NpyType<TraderId::Underlying>::code),
            std::make_pair("price", NpyType<Price::Underlying>::code),
            std::make_pair("volume", NpyType<Volume::Underlying>::code)
        );
    }
};
```

Files are immediately readable in Python:

```python
import numpy as np

# Load directly into memory
performance = np.load('performance.npy')
trades = np.load('trades.npy')

# Access fields by name
pnl = performance['pnl']
prices = trades['price']
```

#### Performance Impact

- **Logging latency**: ~10 nanoseconds per write (just a memory copy)
- **Memory efficiency**: Sparse writes only touch needed bytes; file is sparse-friendly
- **Async I/O**: `madvise()` and `MS_ASYNC` allow kernel to batch writes and reorder for SSD efficiency
- **Direct NumPy compatibility**: Zero parsing overhead; data is immediately usable in Python/Pandas

For 100M simulated events (10k runs × 10k ticks):
- Traditional text logging: ~10 seconds (parsing, buffering, I/O stalls)
- Memory-mapped binary: ~100ms (memory copies + async kernel writeback)

### Benchmark

- **10,000 Monte Carlo runs × 10,000 ticks per run = 100M market events in ~1 second** (Release build, multi-core)
- Single-run 10,000-tick simulation: ~100 microseconds
- Order matching: ~10 nanoseconds per fill (amortized)

This speed is achieved through:
1. Zero allocations in match loop
2. Stack-based order storage
3. Fixed-capacity FIFO queues
4. Compile-time polymorphism (no virtual dispatch)
5. Template specialization for agent-specific logic
6. Parallel Monte Carlo via `std::execution::par`

### Memory Layout & Cache Efficiency

Order log structures use `#pragma pack(1)` for tight packing, enabling direct serialization to NumPy binary format. Hot structures (OrderQueue, OrderBook best-bid/ask) are sized to fit L1 cache limits.

## Metrics & Analysis

**Per-Tick Metrics**:
- Position tracking, realized P&L, equity curves, drawdown

**Trade Statistics**:
- Make/take/fill counts, fill rates, execution quality, slippage

**Risk Metrics**:
- Max drawdown, Sharpe ratio, win rate, volatility

**Monte Carlo Aggregation**:
- Computes quantiles (1%, 5%, 50%, 95%, 99%)
- CVaR at multiple confidence levels
- Distribution statistics

**Binary NumPy Export**:
- `performance.npy`: tick, pnl, position per run
- `quotes.npy`: tick, trader_id, price, volume, side for all orders
- `trades.npy`: tick, buyer_id, seller_id, price, volume per execution
- `monte_carlo.npy`: seed, pnl, max_drawdown per run

## Usage

### Building the Simulator

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Configuration File

Create or modify `config.yml` to define market structure:

```yaml
num_ticks: 10000
seed: 8504
position_limit: 100
runs: 10000

strategy:
  type: PureMarketMaker
  params:
    offset: 1

price_model:
  type: SimpleRandomWalk
  params:
    start_price: 10000
    p: 0.5
    step: 1

makers:
  - type: SymmetricMaker
    count: 1
    params:
      half_spread: 10
      min_volume: 10
      max_volume: 30
  
  - type: SymmetricMaker
    count: 1
    params:
      half_spread: 8
      min_volume: 1
      max_volume: 10

takers:
  - type: RandomTaker
    count: 1
    params:
      trade_prob: 0.05
      min_volume: 1
      max_volume: 10
```

**Config Parameters**:
- `num_ticks`: Number of ticks per simulation run
- `seed`: Random seed for reproducibility
- `position_limit`: Max long/short position size for strategy
- `runs`: Number of Monte Carlo runs (1 = single run, >1 = Monte Carlo mode)
- `strategy`: Your trading algorithm and parameters
- `price_model`: Stochastic price model (SimpleRandomWalk, GaussianRandomWalk, etc.)
- `makers`: List of market maker types and counts
- `takers`: List of market taker types and counts

### Command Line Interface (CLI)

#### Single Run

```bash
./exch-sim --seed 4612
```

**Output** (Single Run):
```
==================================================================================
                            Simulation Results: Run 0                             
==================================================================================
[Config]    Num Ticks:           10000 | Seed: 4612 | (Position Limit:  100)
----------------------------------------------------------------------------------
[PnL]       Total PnL:      22152.00 | Mean PnL:           2.22  (Std:  70.12)
[Orders]    Take Rate:         0.00% | Make Rate:        96.74%
[Fills]     Fill Rate:         5.09% | Avg Fill Vol:       5.27
[Quality]   Avg Slippage:     0.0000 | Fill Quality:       7.00  (Std:   0.00)
[Risk]      Avg Position:      62.57 | Max Drawdown:   -3725.00  (Mean: -995.83)
==================================================================================
```

#### Monte Carlo Simulation

```bash
./exch-sim --runs 10000 --seed 8504
```

**Output** (Monte Carlo):
```
====================================================================================================
                                        Monte Carlo Summary                                         
====================================================================================================
[Config]      Num Ticks:           10000 | Num Runs:10000 | Seed:8504 | (Position Limit:  100)
----------------------------------------------------------------------------------------------------
[PnL]         Mean PnL:    18715.80 | Std Dev:          1020.61 | Win Rate:       100.00%
[Dist]        5th (VaR):   17010.00 | Median:          18732.00 | 95th:          20377.00
[Risk]        CVaR:        16580.52 | Mean Max DD:        -0.00 | Skewness:         -0.05
[General]     Inv Bias:       -0.41 | Avg Slippage:        0.00 | Fill Rate:        5.00%
====================================================================================================
```

#### Runtime Parameter Overrides

Override config parameters without recompilation:

```bash
./exch-sim --set maker.0.half_spread=15 taker.0.trade_prob=0.05
```

#### Additional Options

```bash
./exch-sim --help
```

### Adjusting Fixed-Size Limits

If you get capacity warnings or errors during simulation, adjust the settings in `include/core/settings.hpp`:

```cpp
namespace settings {
    inline constexpr std::size_t INITIAL_ORDER_BOOK_DEPTH = 16;   // Order book levels
    inline constexpr std::size_t MAX_ORDERS_PER_LEVEL = 8;        // Orders per price level
    inline constexpr std::size_t MAX_TRADES_PER_SIDE = 8;         // Concurrent trades
    inline constexpr std::size_t MAX_NUM_LOGS = 1e9;              // Total log entries
};
```

Recompile after changes:

```bash
cd build
cmake ..
make -j$(nproc)
```

## Build System

**CMake 3.16+** with:
- Precompiled headers for faster incremental builds
- Automatic IntelliSense file generation
- Conditional test suite (Debug only)
- ccache integration for CI performance
- Release mode with `-O3 -march=native -mtune=native -flto`

**Dependencies**:
- CLI11 (command-line parsing)
- Embedded YAML parser
- Embedded PCG random number generator
- C++20 standard library features (Concepts, Ranges)

## Code Organization

```
include/
├── core/              # Type-safe primitives, market types
│   ├── primitives.hpp
│   ├── market_types.hpp
│   ├── config.hpp
│   ├── settings.hpp         # Fixed-size configuration
│   └── active_types.hpp     # Compile-time composition
├── model/             # Stochastic price models
│   └── sim_price.hpp
├── bots/              # Maker/taker agent implementations
│   ├── maker.hpp
│   └── taker.hpp
├── sim/               # Core simulation engine
│   ├── simulator.hpp
│   ├── orderbook.hpp
│   ├── metrics.hpp
│   └── sim_builder.hpp
├── interface/         # I/O, validation, CLI
│   ├── printer.hpp
│   ├── cli.hpp
│   └── validator.hpp
├── logging/           # Data export (NumPy)
│   └── log_types.hpp
└── external/          # Embedded libraries
    ├── pcg/           # PCG32 RNG
    └── yaml/          # YAML parser

src/
├── main.cpp
├── cli.cpp
└── sim_runner.cpp

strategy/
└── pure_market_maker.hpp  # User strategy template

config.yml            # Example market configuration
```

## Type Safety & Concepts

**Strong Types** prevent accidental confusion of price/volume/position in function signatures.

**Concepts** enforce compile-time contracts:
```cpp
static_assert(PriceModel<SimpleRandomWalk>);
static_assert(MarketMaker<SymmetricMaker>);
static_assert(MarketTaker<RandomTaker>);
```

Fail to implement the concept? Compilation error, not runtime surprise.

## Key Design Insights

**Extensibility via Composition**: Add a new price model, maker, taker, or strategy by implementing a concept and updating one type alias. The entire system recompiles into a specialized binary.

**Performance Through Type Safety**: Compile-time polymorphism eliminates virtual dispatch, branches, and runtime type checking. You get the flexibility of plugins with the speed of monolithic code.

**Zero-Copy Fast Paths**: Critical operations (order matching, tick iteration) allocate no memory. Fixed-size queues, stack-based order storage, and batch metrics collection ensure deterministic, cache-efficient execution.

**Reproducibility**: Deterministic RNG with seed control enables exact replay of any simulation. Same seed → identical fills, identical P&L. Essential for debugging and research.

Built in modern C++20 with emphasis on composability, type safety, and performance.
