#pragma once

#include "figgie/card.hpp"
#include "figgie/constants.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace figgie {

/// Per-suit card counts for the current game (indices follow `Suit` values).
struct SuitDistribution {
  std::array<std::uint8_t, kSuitCount> count_per_suit{};

  [[nodiscard]] std::uint8_t count(Suit s) const noexcept {
    return count_per_suit[static_cast<std::size_t>(s)];
  }
};

/// Randomly assigns {12,10,10,8} to the four suits (uniform over suit permutations),
/// builds a 40-card multiset, then shuffles card order.
[[nodiscard]] std::vector<Card> make_shuffled_deck(std::mt19937& rng);

/// Same as `make_shuffled_deck`, also returns the distribution used for that deck.
[[nodiscard]] std::pair<std::vector<Card>, SuitDistribution> make_shuffled_deck_with_distribution(
    std::mt19937& rng);

}  // namespace figgie
