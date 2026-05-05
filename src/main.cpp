#include "figgie/constants.hpp"
#include "figgie/game.hpp"

#include <iostream>
#include <random>

int main() {
  std::random_device rd;
  std::mt19937 rng(rd());

  figgie::Game game(rng);

  std::cout << "Starting pot: " << game.pot() << "\n";
  game.run_round(rng);

  return 0;
}
