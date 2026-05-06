#pragma once

#include "figgie/player.hpp"

#include <array>

namespace figgie {

/// Two-sided quoting market-maker.
///
/// On every `act()` the bot loops over all suits, blends the order-book
/// mid-price into an EMA of fair value, skews quotes against its inventory,
/// and posts a bid/ask pair around the resulting fair value.
class MarketMaker : public Player {
 public:
  void act(OrderBook& book, std::array<Player*, kPlayerCount>& players, std::size_t seat_index,
           std::mt19937& rng) override;

 private:
  /// One-time goal-suit inference from the starting hand. Replaces the
  /// fallback fair_values with priors that reflect which suit is most likely
  /// the 8-card goal vs. the 12-card long. Called lazily on the first act().
  void prime_from_hand();

  // Fallback prior (overwritten by prime_from_hand on the first act).
  std::array<float, 4> fair_values = {6.0f, 6.0f, 6.0f, 6.0f};
  const int target_inventory = 2;
  const int spread = 8;
  const float skew_factor = 0.3f;
  // Slow EMA so the goal-suit prior isn't washed out by RB-driven mid noise.
  const float ema_alpha = 0.02f;
  // Hard inventory cap on the bid side. Asks already have a >0-inventory guard.
  const int max_position = 4;

  bool primed_ = false;
};

}  // namespace figgie
