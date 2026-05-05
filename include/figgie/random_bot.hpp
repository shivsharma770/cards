#pragma once

#include "figgie/player.hpp"

namespace figgie {

class RandomBot : public Player {
 public:
  void act(OrderBook& book, std::array<Player*, kPlayerCount>& players, std::size_t seat_index,
           std::mt19937& rng) override;
};

}  // namespace figgie
