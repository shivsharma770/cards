#include "figgie/random_bot.hpp"

#include "figgie/order_book.hpp"

#include <algorithm>
#include <random>

namespace figgie {

void RandomBot::act(OrderBook& book, std::array<Player*, kPlayerCount>& players,
                    std::size_t seat_index, std::mt19937& rng) {
  std::uniform_int_distribution<int> drift_dist(-1, 1);
  for (auto& fair_value : fair_values) {
    fair_value = std::clamp<std::int32_t>(fair_value + drift_dist(rng), 5, 35);
  }

  std::uniform_int_distribution<int> mode_dist(0, 2);
  const int mode = mode_dist(rng);

  if (mode == 0) {
    return;
  }

  if (mode == 1) {
    if (bankroll <= 0) {
      return;
    }
    std::uniform_int_distribution<int> suit_dist(0, static_cast<int>(kSuitCount) - 1);
    const Suit suit = static_cast<Suit>(suit_dist(rng));
    const std::int32_t bid_price = fair_values[static_cast<std::size_t>(suit)] - 2;
    if (bid_price > 0 && bid_price <= bankroll) {
      (void)book.submit_bid(players, seat_index, suit, bid_price);
    }
    return;
  }

  if (hand.empty()) {
    return;
  }

  std::uniform_int_distribution<std::size_t> pick_card(0, hand.size() - 1);
  const Suit suit = hand[pick_card(rng)].suit;

  const std::int32_t ask_price = fair_values[static_cast<std::size_t>(suit)] + 2;
  (void)book.submit_ask(players, seat_index, suit, ask_price);
}

}  // namespace figgie
