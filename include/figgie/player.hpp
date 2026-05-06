#pragma once

#include "figgie/card.hpp"
#include "figgie/constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace figgie {

class OrderBook;

/// Abstract seat at the table: chips + cards, plus behavior for taking a turn.
class Player {
 public:
  virtual ~Player() = default;

  /// Submit orders via `book`. `players` is the full table (needed for settlement).
  virtual void act(OrderBook& book, std::array<Player*, kPlayerCount>& players,
                     std::size_t seat_index, std::mt19937& rng) = 0;

  std::int32_t bankroll{};
  std::vector<Card> hand;
};

}  // namespace figgie
