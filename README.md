# Exchange Simulator

A high-performance, production-grade market microstructure simulator built in C++20. Test algorithmic trading strategies in a realistic exchange environment with full matching engine, order book implementation, and Monte Carlo backtesting.

## Why This Project?

Breaking into quantitative finance requires understanding **real market mechanics**—not just theoretical models. This simulator provides:

- **Realistic matching engine** with price-time priority and proper order crossing logic
- **Full order book implementation** with limit and market orders
- **Agent-based market structure** with makers (liquidity providers) and takers (roaming traders)
- **Stochastic price models** to simulate realistic market conditions
- **Reproducible backtesting** with seed control and Monte Carlo analysis
- **Production-quality code** in C++ that runs fast enough for 1M+ ticks in seconds

This is the kind of system quant firms use to prototype strategies before live deployment. Building and understanding it teaches you core skills that transfer directly to professional work.

## Features

### Core Simulation Engine
- **Order Matching**: Price-time priority with FIFO order queue at each price level
- **Order Book**: Efficient depth tracking with fast bid/ask updates
- **Market Orders**: Execute against best bid/ask with proper partial fill handling
- **Limit Orders**: Rest on book, match incoming opposite-side flow
- **Position Limits**: Risk control that prevents over-leveraging

### Trading Agents
- **Market Makers** (`SymmetricMaker`): Quote bid/ask spreads around mid-price, provide liquidity
- **Market Takers** (`RandomTaker`): Submit random market orders, create flow pressure
- **Strategies** (`PureMarketMaker`): Your algorithm's entry point—offset quotes based on market state

### Price Models
- **Simple Random Walk**: ±step with configurable probability (Brownian motion)
- **Gaussian Random Walk**: Drift + normal increments (log-normal returns)
- Extensible interface for custom price dynamics

### Analysis & Reporting
- **Per-tick metrics**: Position, P&L, order counts
- **Trade statistics**: Fill rates, slippage, average execution price
- **Risk metrics**: Max drawdown, Sharpe ratio, P&L percentiles
- **Monte Carlo backtesting**: Run 1K+ simulations with parameter sensitivity
- **Data export**: NumPy binary format (`.npy`) for offline analysis in Python

## Quick Start

### Prerequisites
- C++20 compiler (GCC 11+, Clang 13+, MSVC 2022+)
- CMake 3.16+
- Intel TBB (for parallel Monte Carlo)
- Optional: GTest for running test suite

**macOS:**
```bash
brew install tbb cmake gcc-13
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt install libtbb-dev cmake g++-11
```

**Windows:**
Use MSVC 2022 with vcpkg: `vcpkg install tbb:x64-windows`

### Build
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

### Run a Single Simulation
```bash
./exch-sim --config ../config.yml --num_ticks 10000 --seed 42
```

**Output:**
```
╔══════════════════════════════════════════════════════════════╗
║                    Simulation Results                        ║
╠══════════════════════════════════════════════════════════════╣
║ P&L (realized):              $1,234.56                       ║
║ Max Drawdown:                -2.34%                          ║
║ Sharpe Ratio:                1.89                            ║
║ Win Rate:                    52.3%                           ║
║ Make/Take Activity:          18.5% / 12.3%                   ║
║ Avg Fill Quality (slippage): 0.82 bps                        ║
╚══════════════════════════════════════════════════════════════╝
```

### Run Monte Carlo (1000 trials)
```bash
./exch-sim --config ../config.yml --runs 1000 --seed 42
```

Generates distribution of P&L, max drawdowns, and risk percentiles.

## Configuration

Edit `config.yml` to define your market and strategy:

```yaml
# Simulation parameters
num_ticks: 50000
seed: 42
position_limit: 100

# Price model (how the stock moves)
price_model:
  type: SimpleRandomWalk
  params:
    start_price: 100.0
    step: 0.5
    p: 0.5  # probability of up move

# Your trading strategy
strategy:
  type: PureMarketMaker
  params:
    offset: 2  # quote 2 ticks from mid

# Market makers (liquidity providers)
makers:
  - count: 1
    params:
      half_spread: 5
      min_volume: 50
      max_volume: 200

# Market takers (noise traders)
takers:
  - count: 1
    params:
      trade_prob: 0.15
      min_volume: 10
      max_volume: 50
```

### Override from Command Line
```bash
./exch-sim --config config.yml \
  --set strategy.offset=3 \
  --set makers.0.half_spread=10 \
  --set price_model.step=1.0
```

## Architecture

```
┌─────────────────────────────────────────────────┐
│              Simulation Loop                    │
│  (tick_handler → matches → strategy → metrics)  │
└──────────────────┬──────────────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
   ┌────▼──────┐      ┌──────▼────┐
   │Order Book │      │ Agents    │
   │(matching) │      │(makers,   │
   │           │      │ takers,   │
   │           │      │ strategy) │
   └───────────┘      └───────────┘
        │                    │
    ┌───┴────────────────────┴───┐
    │                            │
┌───▼──────┐          ┌──────────▼────┐
│Metrics   │          │Price Model    │
│Collection│          │(stochastic)   │
└──────────┘          └───────────────┘
```

### Key Components

| Component | Responsibility |
|-----------|---|
| **OrderBook** | Price-time priority matching, bid/ask depth |
| **Simulator** | Main tick loop, position tracking, order flow |
| **Strategy** | Your algorithm—reads book, submits orders |
| **Makers/Takers** | Market participants creating realistic flow |
| **PriceModel** | Stochastic price evolution |
| **MetricsCollector** | P&L, drawdown, fill statistics |

## Extending for Your Research

### Add a Custom Strategy

1. Create `strategy/my_strategy.hpp`:
```cpp
struct MyStrategy {
    std::optional<OrderRequest> get_order(
        const OrderBook& book,
        const PriceModel& model,
        Position pos,
        PositionLimit limit
    ) {
        auto mid = (book.best_bid() + book.best_ask()) / 2;
        auto spread = book.best_ask() - book.best_bid();
        
        // Your logic here
        if (spread > 10) {
            return OrderRequest::limit(
                TraderId{0}, 
                mid - 1, 
                Volume{50}, 
                Side::BID
            );
        }
        return std::nullopt;
    }
};
```

2. Update `include/core/active_types.hpp`:
```cpp
#include "my_strategy.hpp"
using ActiveStrategy = MyStrategy;
```

3. Recompile and test.

### Add Custom Price Model

Implement the `PriceModel` concept (requires `next_price()`, `current_price()`):

```cpp
struct MyPriceModel {
    MyPriceModel(pcg32 rng, ConfigParams p) : rng(rng), params(p) {}
    
    double next_price() {
        // Your price dynamics
        return current;
    }
    
    double current_price() const { return current; }
    
private:
    pcg32 rng;
    ConfigParams params;
    double current;
};
```

### Export Data for Analysis

Run with `--out logs/`:
```bash
./exch-sim --config config.yml --out logs/ --runs 100
```

Generates:
- `performance.npy`: (tick, pnl, position) arrays per run
- `quotes.npy`: (tick, trader_id, price, volume, side) for all quotes
- `trades.npy`: (tick, buyer_id, seller_id, price, volume) executed trades
- `monte_carlo.npy`: (seed, pnl, max_drawdown) aggregates

Load in Python:
```python
import numpy as np
perf = np.load('logs/latest/performance.npy')
print(perf.shape)  # (num_ticks, 3)
pnl = perf[:, 1]
```

## Testing

Run the test suite:
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
ctest --output-on-failure
```

Tests cover:
- **Order matching**: Crossing logic, partial fills, FIFO priority
- **Price models**: Reproducibility, distribution properties
- **Maker/Taker behavior**: Probability bounds, volume constraints
- **Full simulator**: End-to-end run consistency

## Performance

Built for speed—optimized for high-frequency backtesting:

- **50K ticks**: ~100ms (single run)
- **1M ticks**: ~2 seconds (single run)
- **100 Monte Carlo runs × 50K ticks**: ~10 seconds (parallel)

Release build flags:
- `-O3 -march=native -mtune=native`: CPU-specific optimizations
- `-flto`: Link-time optimization
- PCH (precompiled headers): Faster rebuilds
- `ccache` support for CI

## Learning Path for Quant Interviews

This codebase teaches:

1. **Market Microstructure** (weeks 1-2)
   - How order books work and why they match this way
   - Price-time priority and fairness
   - Liquidity and spread dynamics

2. **Strategy Development** (weeks 2-4)
   - Reading live order book state
   - Optimal order placement (offset logic)
   - Risk management via position limits
   - Execution algorithms (volume scheduling)

3. **Backtesting & Analysis** (weeks 3-5)
   - Metrics that matter (Sharpe, drawdown, fill rates)
   - Monte Carlo for risk assessment
   - Parameter sensitivity and robustness
   - Avoiding look-ahead bias

4. **Systems & Production Code** (weeks 4-6)
   - Type-safe C++ with concepts
   - High-performance order matching
   - Configuration management
   - Reproducibility (seeding, logging)

**Common interview questions you'll be ready for:**
- "How would you market-make this stock?" → Build a custom strategy
- "What's your max loss?" → Run Monte Carlo, compute VaR
- "How fast can you execute?" → Benchmark against the simulator
- "What if volatility spikes?" → Adjust price model params and retest

## Project Structure

```
exchange-simulator/
├── CMakeLists.txt              # Build configuration
├── config.yml                  # Example simulation config
├── include/
│   ├── core/
│   │   ├── primitives.hpp      # Price, Volume, TraderId, etc.
│   │   ├── market_types.hpp    # OrderBook, OrderRequest, Side
│   │   ├── settings.hpp        # Tuning constants
│   │   └── config.hpp          # YAML config parsing
│   ├── model/
│   │   └── sim_price.hpp       # Price models (random walks)
│   ├── bots/
│   │   ├── maker.hpp           # SymmetricMaker
│   │   └── taker.hpp           # RandomTaker
│   ├── sim/
│   │   ├── simulator.hpp       # Core simulation engine
│   │   ├── orderbook.hpp       # Order matching logic
│   │   ├── metrics.hpp         # P&L, drawdown, risk calcs
│   │   └── sim_builder.hpp     # Config → Simulator wiring
│   ├── interface/
│   │   ├── printer.hpp         # Console output formatting
│   │   └── validator.hpp       # Config validation
│   ├── logging/
│   │   └── log_types.hpp       # Structured log schema
│   └── external/
│       ├── pcg/                # PCG random number generator
│       └── yaml/               # YAML config library
├── src/
│   ├── main.cpp               # Entry point
│   ├── cli.cpp                # CLI argument parsing
│   └── sim_runner.cpp         # Single/Monte Carlo orchestration
├── strategy/
│   └── pure_market_maker.hpp  # Example user strategy
└── tests/
    ├── test_matching.cpp      # Order book unit tests
    ├── test_price_models.cpp  # Price model tests
    ├── test_maker.cpp         # Maker behavior tests
    ├── test_taker.cpp         # Taker behavior tests
    ├── test_strategy.cpp      # Strategy tests
    └── test_simulator_run.cpp # End-to-end integration tests
```

## Advanced Topics

### Reproducibility & Seeding
All randomness uses PCG32 with explicit seed. Same seed = identical order fills and P&L:
```bash
./exch-sim --config config.yml --seed 12345  # Run 1
./exch-sim --config config.yml --seed 12345  # Run 2 → Identical
```

### Parallel Monte Carlo
Simulator uses Intel TBB for parallel runs:
```bash
./exch-sim --config config.yml --runs 1000
```
Auto-scales to all CPU cores.

### Order Lifecycle Tracing
Enable logging to trace individual orders through the book:
```bash
./exch-sim --config config.yml --out logs/
# Inspect trades.npy and quotes.npy to see order flow
```

## FAQ

**Q: Can I use this for live trading?**  
A: No. This is a simulator for research and interview prep. For live trading, use professional frameworks (QuantConnect, Interactive Brokers, etc.).

**Q: How do I add realistic market effects (fees, latency, slippage)?**  
A: All are modular. Slippage can be added to the matching logic; latency to order routing. See the `Strategy` interface—your `get_order()` could adjust for estimated fill price.

**Q: Why C++ and not Python?**  
A: Speed. Python + NumPy is great for analysis *post-simulation*. C++ is for the simulation loop itself—this runs 100x faster than naive Python, enabling rapid iteration.

**Q: Can I backtest on real market data?**  
A: The simulator uses synthetic price paths. To backtest on real data, replace `PriceModel` with a data reader. Order flow (makers/takers) would still be synthetic, but realistic enough for strategy evaluation.

**Q: How do I cite this in my interview prep?**  
A: Show the code, describe what you learned, and demonstrate one custom addition (strategy or metric). Interviewers care about depth—not just that you ran it.

## Resources

- **Market Microstructure**: "Market Microstructure in Practice" by Cliff Asness et al.
- **Algorithmic Trading**: "Algorithmic Trading" by Ernie Chan
- **C++ Performance**: "Efficient C++" by Herb Sutter
- **Backtesting Pitfalls**: "Advances in Financial Machine Learning" by Marcos López de Prado

## Contributing

This is a learning project. Contributions welcome:
- Custom price models or strategies
- Additional performance metrics
- Better documentation or examples
- Bug fixes or optimizations

## License

MIT License—use freely for education and research.

---

## Getting Help

- **Build issues?** Check CMakeLists.txt dependencies and compiler version
- **Wrong results?** Verify config.yml matches your intent; check test cases for expected behavior
- **Performance?** Profile with `-O3 -march=native`; check order queue size limits in `settings.hpp`

Good luck breaking into quant. The work starts here. 📈
