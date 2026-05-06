#include "figgie/market_maker.hpp"

#include "figgie/card.hpp"
#include "figgie/order_book.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace figgie {

namespace {

constexpr std::int32_t kAskUpperBound = 40;

// Goal-suit inference priors.
//
// In this Figgie variant:
//   - One suit has 12 cards (the "long" suit; never the goal).
//   - Two suits have 10 cards each (mediums; not the goal).
//   - One suit has 8 cards (the "short" suit, which IS the goal — same colour
//     partner of the long suit).
//
// At deal time each player gets 10 of the 40 cards. Expected counts in your
// hand: long ~3.0, medium ~2.5, short/goal ~2.0. So the suit you hold the
// MOST of is most likely the long suit, and the goal is its partner.
//
// Per the backtest's P&L formula, a held goal card is worth 10 chips. We
// price the goal slightly above that (12) to leave room for the half-spread
// on the bid side without overpaying. Long and mediums are non-goal and
// settle at ~0, so we price them low.
constexpr float kPriorGoal = 12.0f;
constexpr float kPriorLong = 2.0f;
constexpr float kPriorMedium = 4.0f;

}  // namespace

void MarketMaker::prime_from_hand() {
  std::array<int, kSuitCount> counts{};
  for (const auto& c : hand) {
    ++counts[static_cast<std::size_t>(c.suit)];
  }

  std::size_t long_idx = 0;
  for (std::size_t i = 1; i < kSuitCount; ++i) {
    if (counts[i] > counts[long_idx]) {
      long_idx = i;
    }
  }
  const Suit long_suit = static_cast<Suit>(long_idx);
  const Suit goal_suit = partner_suit_same_color(long_suit);

  for (std::size_t i = 0; i < kSuitCount; ++i) {
    const Suit s = static_cast<Suit>(i);
    if (s == goal_suit) {
      fair_values[i] = kPriorGoal;
    } else if (s == long_suit) {
      fair_values[i] = kPriorLong;
    } else {
      fair_values[i] = kPriorMedium;
    }
  }
}

void MarketMaker::act(OrderBook& book, std::array<Player*, kPlayerCount>& players,
                      std::size_t seat_index, std::mt19937& /*rng*/) {
  if (!primed_) {
    prime_from_hand();
    primed_ = true;
  }

  for (std::size_t suit_idx = 0; suit_idx < kSuitCount; ++suit_idx) {
    const Suit suit = static_cast<Suit>(suit_idx);

    // Tape reading: blend the current book mid into our EMA. With
    // ema_alpha = 0.02 the prior dominates for ~30+ updates, so the
    // goal-suit inference persists while still allowing slow adaptation.
    const float signal = book.get_mid_price(suit);
    if (signal > 0.0f) {
      fair_values[suit_idx] =
          fair_values[suit_idx] * (1.0f - ema_alpha) + signal * ema_alpha;
    }

    int current_inventory = 0;
    for (const auto& c : hand) {
      if (c.suit == suit) {
        ++current_inventory;
      }
    }
    const float skew =
        static_cast<float>(current_inventory - target_inventory) * skew_factor;

    const float fv = fair_values[suit_idx];
    const float half_spread = static_cast<float>(spread) / 2.0f;
    auto bid_price = static_cast<std::int32_t>(std::lround(fv - half_spread - skew));
    auto ask_price = static_cast<std::int32_t>(std::lround(fv + half_spread - skew));

    if (bankroll > 0) {
      bid_price = std::clamp<std::int32_t>(bid_price, 1, bankroll);
    }
    ask_price = std::clamp<std::int32_t>(ask_price, 1, kAskUpperBound);

    // Hard position cap on the bid side: we never accumulate beyond
    // max_position cards of any one suit, regardless of how negative the
    // skew goes.
    if (bankroll >= bid_price && current_inventory < max_position) {
      (void)book.submit_bid(players, seat_index, suit, bid_price);
    }
    if (current_inventory > 0) {
      (void)book.submit_ask(players, seat_index, suit, ask_price);
    }
  }
}

}  // namespace figgie
