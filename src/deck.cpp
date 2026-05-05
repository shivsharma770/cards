#include "figgie/deck.hpp"

#include <algorithm>
#include <utility>

namespace figgie {

namespace {

void assign_random_counts(std::array<std::uint8_t, kSuitCount>& out, std::mt19937& rng) {
  std::array<Suit, kSuitCount> order{Suit::Spades, Suit::Clubs, Suit::Hearts, Suit::Diamonds};
  std::shuffle(order.begin(), order.end(), rng);

  constexpr std::array<std::uint8_t, kSuitCount> deck_counts{
      kLongSuitCount,
      kMediumSuitCount,
      kMediumSuitCount,
      kShortSuitCount,
  };

  for (std::size_t i = 0; i < kSuitCount; ++i) {
    out[static_cast<std::size_t>(order[i])] = deck_counts[i];
  }
}

std::vector<Card> build_deck_from_distribution(const SuitDistribution& dist) {
  std::vector<Card> deck;
  deck.reserve(kDeckSize);

  for (std::size_t si = 0; si < kSuitCount; ++si) {
    const auto suit = static_cast<Suit>(si);
    const auto n = dist.count_per_suit[si];
    for (std::uint8_t k = 0; k < n; ++k) {
      deck.push_back(Card{suit});
    }
  }

  return deck;
}

}  // namespace

std::vector<Card> make_shuffled_deck(std::mt19937& rng) {
  return make_shuffled_deck_with_distribution(rng).first;
}

std::pair<std::vector<Card>, SuitDistribution> make_shuffled_deck_with_distribution(
    std::mt19937& rng) {
  SuitDistribution dist{};
  assign_random_counts(dist.count_per_suit, rng);

  auto deck = build_deck_from_distribution(dist);
  std::shuffle(deck.begin(), deck.end(), rng);

  return {std::move(deck), dist};
}

}  // namespace figgie
