# Exchange Simulator

A high-performance market microstructure simulator built in modern C++20. Implements a complete electronic exchange with matching engine, order book management, pluggable agent-based market participants, and stochastic price models—all designed for zero-copy fast paths and compile-time composability.

## Project Overview

Exchange Simulator is a complete electronic market system written from scratch in C++. The core contribution is demonstrating how to build a highly extensible, high-performance system where any market participant (price model, maker/taker bot, user strategy) can be freely mixed and matched at compile time without performance compromise.

## Core Architecture

### Order Matching Engine

- **Price-Time Priority Matching**: Implements standard exchange rules with FIFO order queues at each price level
- **Order Crossing Logic**: Intelligent bid-ask crossing detection (BID ≥ ASK for matches)
- **Partial Fill Handling**: Supports both complete and partial order fills with proper settlement
- **Market & Limit Orders**: Full support for market orders (execute or nothing) and limit orders (rest on book)
- **FIFO Order Queue**: Custom fixed-size array-based queue with O(1) operations—no dynamic allocation in hot paths
- **Order Lifecycle**: Complete tracking from submission through fill to settlement

### Order Book Management

Efficient depth tracking with automatic cleanup of empty price levels. Fast best-bid/best-ask queries and proper handling of partial fills. The book is rebuilt and snapshotted for metrics collection without disrupting the matching loop.

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

The framework ships with configurable maker implementations, but the key design point is that **new maker strategies can be added by implementing this concept**. Mix multiple different maker types in a single simulation—each with independent parameters and behavior.

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

Each combination compiles into a single, type-safe binary with **zero runtime polymorphism overhead**. No virtual functions, no branching on agent type—the compiler generates specialized code for your exact configuration.

### Configuration & Wiring

YAML configuration declares the market structure (how many of each agent type, their parameters). The `SimBuilder` pattern reads the config and constructs a fully wired `Simulator<ActiveMakers, ActiveTakers, ActiveStrategy, ActivePriceModel>` with all dependencies resolved:

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

Every order queue is pre-allocated. Order placement never triggers dynamic memory allocation—critical for microsecond-level consistency.

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

**Multi-Threaded Monte Carlo**

Monte Carlo runs (1000+) are embarrassingly parallel—each run is independent. The simulator uses Intel TBB for automatic work distribution across all cores:

```cpp
tbb::parallel_for(0, num_runs, [&](int run_idx) {
    auto result = single_run(run_idx);  // Each core gets independent run
});
```

Single-core runs dominate when num_runs < num_cores. Each core runs its own independent simulation without coordination.

### Benchmark

- **10,000 Monte Carlo runs × 10,000 ticks per run = 100M market events in 1 second** (Release build, multi-core)
- Single-run 10,000-tick simulation: ~100 microseconds
- Order matching: ~10 nanoseconds per fill (amortized)

This speed is achieved through:
1. Zero allocations in match loop
2. Stack-based order storage
3. Fixed-capacity FIFO queues
4. Compile-time polymorphism (no virtual dispatch)
5. Template specialization for agent-specific logic
6. Parallel Monte Carlo via TBB

### Memory Layout & Cache Efficiency

Order log structures use `#pragma pack(1)` for tight packing, enabling direct serialization to NumPy binary format. Hot structures (OrderQueue, OrderBook best-bid/ask) are sized to fit L1 cache lines. Avoid cache misses in the critical path.

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

## Build System

**CMake 3.16+** with:
- Precompiled headers for faster incremental builds
- Automatic IntellISense file generation
- Conditional test suite (Debug only)
- ccache integration for CI performance
- Release mode with `-O3 -march=native -mtune=native -flto`

**Dependencies**:
- Intel TBB (threading)
- CLI11 (command-line parsing)
- Embedded YAML parser
- Embedded PCG random number generator

## Code Organization

```
include/
├── core/              # Type-safe primitives, market types
│   ├── primitives.hpp
│   ├── market_types.hpp
│   ├── config.hpp
│   └── active_types.hpp  # Compile-time composition
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

**Extensibility via Composition**: Add a new price model, maker, taker, or strategy by implementing a concept and updating one type alias. The entire system recompiles into a specialized binary. No runtime overhead, no plugin system complexity.

**Performance Through Type Safety**: Compile-time polymorphism eliminates virtual dispatch, branches, and runtime type checking. You get the flexibility of plugins with the speed of monolithic code.

**Zero-Copy Fast Paths**: Critical operations (order matching, tick iteration) allocate no memory. Fixed-size queues, stack-based order storage, and batch metrics collection ensure deterministic, microsecond-level latency.

**Reproducibility**: Deterministic RNG with seed control enables exact replay of any simulation. Same seed → identical fills, identical P&L. Essential for debugging and research.

Built in modern C++20 with emphasis on composability, type safety, and performance.
