#include "figgie/random_bot.hpp"

#include "figgie/order_book.hpp"

#include <algorithm>
#include <random>

namespace figgie {

void RandomBot::act(OrderBook& book, std::array<Player*, kPlayerCount>& players,
                    std::size_t seat_index, std::mt19937& rng) {
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
    std::uniform_int_distribution<std::int32_t> price_dist(1, bankroll);
    const std::int32_t price = price_dist(rng);
    book.submit_bid(players, seat_index, suit, price);
    return;
  }

  if (hand.empty()) {
    return;
  }

  std::uniform_int_distribution<std::size_t> pick_card(0, hand.size() - 1);
  const Suit suit = hand[pick_card(rng)].suit;

  const std::int32_t max_price = std::max<std::int32_t>(1, bankroll);
  std::uniform_int_distribution<std::int32_t> price_dist(1, max_price);
  const std::int32_t price = price_dist(rng);

  book.submit_ask(players, seat_index, suit, price);
}

}  // namespace figgie
