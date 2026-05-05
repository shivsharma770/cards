#pragma once

#include "figgie/card.hpp"
#include "figgie/constants.hpp"
#include "figgie/deck.hpp"
#include "figgie/order_book.hpp"
#include "figgie/player.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <random>

namespace figgie {

/// Owns table state for one round: hands, bankrolls after ante, pot, and the hidden goal suit.
class Game {
 public:
  explicit Game(std::mt19937& rng);

  /// Runs `kTicksPerRound` ticks (each bot acts once per tick), logs trades, then settles the round.
  void run_round(std::mt19937& rng);

  [[nodiscard]] std::int32_t pot() const noexcept { return pot_; }

  [[nodiscard]] std::array<std::unique_ptr<Player>, kPlayerCount>& players() noexcept {
    return players_;
  }

  [[nodiscard]] const std::array<std::unique_ptr<Player>, kPlayerCount>& players() const noexcept {
    return players_;
  }

  /// Raw pointers to each seat (for `Player::act` / `OrderBook` submission).
  [[nodiscard]] std::array<Player*, kPlayerCount> player_ptr_table() noexcept {
    std::array<Player*, kPlayerCount> table{};
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
      table[i] = players_[i].get();
    }
    return table;
  }

 private:
  void initialize_round(std::mt19937& rng);

  void settle_round();

  /// Goal suit is the partner suit (same color) of whichever suit has 12 cards.
  [[nodiscard]] static Suit compute_goal_suit(const SuitDistribution& dist) noexcept;

  std::array<std::unique_ptr<Player>, kPlayerCount> players_{};
  OrderBook order_book_{};
  std::int32_t pot_{};
  Suit goal_suit_{};  ///< Secret until end-of-round settlement.
};

}  // namespace figgie
