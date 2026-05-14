# Figgie Exchange — C++ Trading Simulator

A low-latency C++ simulation of [Jane Street's Figgie](https://www.figgie.com/), a zero-sum card game designed to teach market-making intuition. The engine implements a full continuous limit order book (LOB), a heterogeneous agent economy, and a 100,000-game Monte Carlo backtester.

---

## What is Figgie?

Figgie is a 4-player trading game built around hidden information and probability inference:

- A 40-card deck is dealt: one suit has **12 cards** (the "long" suit), two have **10**, and one has **8** (the "short" suit — this is the **goal suit**).
- The goal suit is the **same-color partner** of the long suit (e.g. if Spades is long, Clubs is goal).
- Players trade cards in a continuous double-auction market. At round end, each goal-suit card pays **10 chips** from the pot, and the player holding the **most** goal cards wins the remaining pot.
- No one is told which suit is the goal — it must be inferred from hand composition and order flow.

The game is a microcosm of real market-making: you must quote two-sided markets under uncertainty, manage inventory risk, and update your beliefs as the book reveals information.

---

## Architecture

```
cards-main/
├── include/figgie/
│   ├── order_book.hpp      # Central LOB: per-suit bid/ask stacks, fill logic
│   ├── market_maker.hpp    # Two-sided quoting bot with EMA + inventory skew
│   ├── random_bot.hpp      # Noise trader: bounded random-walk pricing
│   ├── game.hpp            # Round lifecycle: deal, tick loop, settlement
│   ├── deck.hpp            # Shuffled deck generation with suit distribution
│   ├── card.hpp            # Card / suit primitives
│   ├── player.hpp          # Abstract player base
│   └── constants.hpp       # Game parameters (chip values, tick counts, etc.)
└── src/
    ├── main.cpp            # 100k-game Monte Carlo backtest harness
    ├── game.cpp            # Round runner with ANSI trade ticker
    ├── order_book.cpp      # LOB matching engine
    ├── market_maker.cpp    # MM pricing logic
    ├── random_bot.cpp      # Noise trader logic
    └── deck.cpp            # Deck shuffling
```

---

## Key Algorithms

### Limit Order Book (`order_book.cpp`)

A per-suit continuous double auction. Each suit maintains a sorted bid stack (descending) and ask stack (ascending). On every `submit_bid` or `submit_ask`:

1. Check for a crossing quote on the opposite side.
2. If a fill occurs, transfer the card and chips between players.
3. Per official Figgie rules: **all resting orders on every suit are cleared** after any fill — preventing stale quotes from persisting after price discovery.

Orders are assigned monotonically increasing IDs and all fills are logged to a trade tape for downstream analysis.

### Market Maker (`market_maker.cpp`)

A two-sided quoting bot that runs the following pipeline on every tick, per suit:

**1. Goal-suit inference from hand (one-time prior)**

On the first tick, the MM inspects its starting hand. The suit it holds the most cards of is most likely the 12-card long suit. The goal suit is the same-color partner. This sets asymmetric priors:

| Inferred role | Fair value prior |
|---------------|-----------------|
| Goal suit     | 12 chips         |
| Long suit     | 2 chips          |
| Medium suits  | 4 chips          |

**2. EMA-based fair value update (tape reading)**

Each tick, the MM reads the current book mid-price for each suit and blends it into a running exponential moving average:

```
FV_t = FV_{t-1} × (1 − α) + mid_t × α      α = 0.02
```

The slow alpha (α = 0.02) ensures the goal-suit prior persists for ~30+ ticks before order-flow noise can wash it out — modeling the real market-maker tradeoff between prior conviction and signal responsiveness.

**3. Inventory skew**

Quote prices are skewed relative to fair value based on current inventory vs. target:

```
skew = (current_inventory − target_inventory) × skew_factor
bid  = round(FV − half_spread − skew)
ask  = round(FV + half_spread − skew)
```

Excess inventory pushes the ask down (offload pressure) and bids up (accumulation pressure), preventing runaway directional exposure.

**4. Hard position cap**

Bids are suppressed entirely once inventory exceeds `max_position = 4` cards of a single suit, regardless of skew direction.

### Noise Trader / Random Bot (`random_bot.cpp`)

Each tick, the RandomBot:
- Applies a ±1 random walk to its per-suit fair value estimates (bounded [5, 35]).
- With equal probability: does nothing, posts a bid 2 below its fair value, or posts an ask 2 above its fair value on a randomly chosen held card.

This generates realistic uninformed order flow that the MM can trade against.

---

## Backtest

`main.cpp` runs 100,000 independent games (1 MM vs 3 RandomBots, 1,000 ticks each) and reports:

```
========== 100000-Game Backtest Summary (1 MMs vs 3 RBs) ==========
Games played:    100000
MM samples:      100000
Elapsed:         X.XX s
Wins:            XXXXX  (XX.X%)
Losses:          XXXXX  (XX.X%)
Average P&L:     +X.XX chips/MM-game
Std deviation:   XX.XX chips
Distribution:    p10=XX  p25=XX  median=XX  p75=XX  p90=XX
```

---

## Build

**Requirements:** C++17 compiler, CMake ≥ 3.14

```bash
git clone https://github.com/shivsharma770/cards
cd cards
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/figgie_sim
```

---

## What's Next

- **Alpha Bot:** A Bayesian agent that maintains a full posterior over the goal-suit distribution, updating on every observed trade and quote using Bayes' theorem rather than a point-estimate EMA. Expected to outperform the MM significantly against informed opponents.
- **Multi-round session tracking** with bankroll persistence across rounds.
- **Bot tournament** harness for head-to-head ELO-style ranking.
