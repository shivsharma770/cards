#pragma once

#include "figgie/player.hpp"

#include <array>
#include <cstdint>

namespace figgie {

class RandomBot : public Player {
 public:
  void act(OrderBook& book, std::array<Player*, kPlayerCount>& players, std::size_t seat_index,
           std::mt19937& rng) override;

  std::array<std::int32_t, 4> fair_values = {15, 15, 15, 15};
};

}  // namespace figgie
