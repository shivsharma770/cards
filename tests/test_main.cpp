// Comprehensive test suite for the figgie bot, order book, and game flow.
//
// Uses <cassert> exclusively. The CMake target compiles with -UNDEBUG so that
// asserts always fire, even in Release builds.

#include "figgie/card.hpp"
#include "figgie/constants.hpp"
#include "figgie/game.hpp"
#include "figgie/order_book.hpp"
#include "figgie/player.hpp"
#include "figgie/random_bot.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

namespace {

using figgie::Card;
using figgie::Game;
using figgie::OrderBook;
using figgie::Player;
using figgie::RandomBot;
using figgie::Suit;
using figgie::kAnteAmount;
using figgie::kCardsPerPlayer;
using figgie::kInitialPot;
using figgie::kPlayerCount;
using figgie::kRoundsPerGame;
using figgie::kStartingBankroll;
using figgie::kSuitCount;

// ============================================================================
// Fixtures and helpers
// ============================================================================

struct BotTable {
  std::array<std::unique_ptr<RandomBot>, kPlayerCount> bots;
  std::array<Player*, kPlayerCount> table{};
  OrderBook book;

  BotTable() {
    for (auto& b : bots) {
      b = std::make_unique<RandomBot>();
    }
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
      table[i] = bots[i].get();
    }
  }

  void act(std::size_t seat, std::mt19937& rng) {
    bots[seat]->act(book, table, seat, rng);
  }
};

[[nodiscard]] std::size_t total_orders(
    const std::array<std::vector<OrderBook::Order>, kSuitCount>& side) {
  std::size_t n = 0;
  for (const auto& s : side) {
    n += s.size();
  }
  return n;
}

[[nodiscard]] std::size_t orders_for_player(
    const std::array<std::vector<OrderBook::Order>, kSuitCount>& side, std::size_t player) {
  std::size_t n = 0;
  for (const auto& s : side) {
    for (const auto& o : s) {
      if (o.player == player) {
        ++n;
      }
    }
  }
  return n;
}

// Game logs heavily to stdout. Silence it for the duration of one test.
struct CoutSilencer {
  std::stringstream sink;
  std::streambuf* prev;
  CoutSilencer() : prev(std::cout.rdbuf(sink.rdbuf())) {}
  ~CoutSilencer() { std::cout.rdbuf(prev); }
};

// ============================================================================
// 1. fair_values: initial state and random-walk invariants
// ============================================================================

void test_fair_values_initialised_to_15() {
  RandomBot bot;
  for (auto v : bot.fair_values) {
    assert(v == 15);
  }
}

void test_fair_values_stay_within_clamp_bounds() {
  std::mt19937 rng(1);
  BotTable t;
  t.bots[0]->bankroll = 10'000;
  t.bots[0]->hand = {Card{Suit::Spades}, Card{Suit::Hearts}, Card{Suit::Diamonds},
                     Card{Suit::Clubs}};
  for (int i = 0; i < 50'000; ++i) {
    t.act(0, rng);
    for (auto v : t.bots[0]->fair_values) {
      assert(v >= 5 && v <= 35);
    }
  }
}

void test_fair_values_drift_by_at_most_one_per_act() {
  std::mt19937 rng(2);
  BotTable t;
  t.bots[0]->bankroll = 10'000;
  t.bots[0]->hand = {Card{Suit::Spades}};
  for (int i = 0; i < 10'000; ++i) {
    auto before = t.bots[0]->fair_values;
    t.act(0, rng);
    for (std::size_t s = 0; s < kSuitCount; ++s) {
      const auto delta = std::abs(t.bots[0]->fair_values[s] - before[s]);
      assert(delta <= 1);
    }
  }
}

void test_fair_values_actually_drift_over_time() {
  std::mt19937 rng(3);
  BotTable t;
  t.bots[0]->bankroll = 10'000;
  t.bots[0]->hand = {Card{Suit::Spades}};
  for (int i = 0; i < 1'000; ++i) {
    t.act(0, rng);
  }
  bool any_changed = false;
  for (auto v : t.bots[0]->fair_values) {
    if (v != 15) {
      any_changed = true;
      break;
    }
  }
  assert(any_changed);
}

// ============================================================================
// 2. Bot pricing formulas
// ============================================================================

void test_bid_price_within_fair_value_minus_two_band() {
  std::mt19937 rng(4);
  BotTable t;
  t.bots[0]->bankroll = 10'000;       // never under-funded
  t.bots[0]->hand = {};               // empty: forces bid-only behavior
  for (int i = 0; i < 2'000; ++i) {
    t.act(0, rng);
  }
  for (std::size_t s = 0; s < kSuitCount; ++s) {
    for (const auto& o : t.book.bids()[s]) {
      // bid_price = fair_values[suit] - 2, with fair_values clamped to [5, 35].
      assert(o.price >= 3 && o.price <= 33);
    }
  }
  assert(total_orders(t.book.bids()) > 0);  // bot did try to buy at least once
}

void test_ask_price_within_fair_value_plus_two_band() {
  std::mt19937 rng(5);
  BotTable t;
  t.bots[0]->bankroll = 0;            // can't bid: forces ask-only behavior
  for (auto s : {Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs}) {
    for (int k = 0; k < 200; ++k) {
      t.bots[0]->hand.push_back(Card{s});
    }
  }
  for (int i = 0; i < 2'000; ++i) {
    t.act(0, rng);
  }
  for (std::size_t s = 0; s < kSuitCount; ++s) {
    for (const auto& o : t.book.asks()[s]) {
      // ask_price = fair_values[suit] + 2, with fair_values clamped to [5, 35].
      assert(o.price >= 7 && o.price <= 37);
    }
  }
  assert(total_orders(t.book.asks()) > 0);
}

// ============================================================================
// 3. Bot guards: broke / under-funded / empty hand
// ============================================================================

void test_broke_bot_never_submits_a_bid() {
  std::mt19937 rng(6);
  BotTable t;
  t.bots[0]->bankroll = 0;
  t.bots[0]->hand = {Card{Suit::Spades}};
  for (int i = 0; i < 5'000; ++i) {
    t.act(0, rng);
    assert(orders_for_player(t.book.bids(), 0) == 0);
  }
}

void test_low_bankroll_caps_bid_price() {
  std::mt19937 rng(7);
  // Sweep tight bankroll values. Other bots have nothing — no trades fill,
  // so bids just rest, making them easy to inspect.
  for (std::int32_t br = 1; br <= 50; ++br) {
    BotTable t;
    t.bots[0]->bankroll = br;
    t.bots[0]->hand = {};
    for (int i = 0; i < 200; ++i) {
      const auto bankroll_before = t.bots[0]->bankroll;
      t.act(0, rng);
      for (std::size_t s = 0; s < kSuitCount; ++s) {
        for (const auto& o : t.book.bids()[s]) {
          if (o.player == 0) {
            assert(o.price <= bankroll_before);
          }
        }
      }
    }
  }
}

void test_empty_hand_bot_never_submits_an_ask() {
  std::mt19937 rng(8);
  BotTable t;
  t.bots[0]->bankroll = 0;
  t.bots[0]->hand = {};
  for (int i = 0; i < 5'000; ++i) {
    t.act(0, rng);
    assert(orders_for_player(t.book.asks(), 0) == 0);
  }
}

void test_bot_only_sells_suits_it_holds() {
  std::mt19937 rng(9);
  BotTable t;
  t.bots[0]->bankroll = 0;
  for (int k = 0; k < 50; ++k) {
    t.bots[0]->hand.push_back(Card{Suit::Spades});
  }
  for (int k = 0; k < 50; ++k) {
    t.bots[0]->hand.push_back(Card{Suit::Hearts});
  }
  for (int i = 0; i < 2'000; ++i) {
    t.act(0, rng);
  }
  // Player 0 must never have a resting ask in suits they don't hold.
  for (const auto& o : t.book.asks()[static_cast<std::size_t>(Suit::Clubs)]) {
    assert(o.player != 0);
  }
  for (const auto& o : t.book.asks()[static_cast<std::size_t>(Suit::Diamonds)]) {
    assert(o.player != 0);
  }
}

void test_idle_bot_does_nothing_when_resourceless() {
  std::mt19937 rng(10);
  BotTable t;
  t.bots[0]->bankroll = 0;
  t.bots[0]->hand = {};
  for (int i = 0; i < 5'000; ++i) {
    t.act(0, rng);
  }
  assert(total_orders(t.book.bids()) == 0);
  assert(total_orders(t.book.asks()) == 0);
}

// ============================================================================
// 4. Randomness: modes, suits, cards
// ============================================================================

void test_bot_takes_all_three_modes_over_many_turns() {
  std::mt19937 rng(11);
  BotTable t;
  t.bots[0]->bankroll = 10'000;
  auto restock = [&]() {
    t.bots[0]->hand.clear();
    for (auto s : {Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs}) {
      for (int k = 0; k < 4; ++k) {
        t.bots[0]->hand.push_back(Card{s});
      }
    }
  };
  int bids = 0;
  int asks = 0;
  int no_ops = 0;
  for (int i = 0; i < 6'000; ++i) {
    restock();
    t.book = OrderBook{};  // fresh book per turn so we can read off the mode unambiguously
    t.act(0, rng);
    if (total_orders(t.book.bids()) == 1) {
      ++bids;
    } else if (total_orders(t.book.asks()) == 1) {
      ++asks;
    } else {
      ++no_ops;
    }
  }
  // Each mode should fire ~1/3 of the time. Allow a generous margin.
  assert(bids > 1'500);
  assert(asks > 1'500);
  assert(no_ops > 1'500);
}

void test_bot_bids_across_all_four_suits() {
  std::mt19937 rng(12);
  BotTable t;
  t.bots[0]->bankroll = 10'000;
  t.bots[0]->hand = {};
  std::array<bool, kSuitCount> seen{};
  for (int i = 0; i < 5'000; ++i) {
    t.book = OrderBook{};
    t.act(0, rng);
    for (std::size_t s = 0; s < kSuitCount; ++s) {
      if (!t.book.bids()[s].empty()) {
        seen[s] = true;
      }
    }
  }
  for (auto b : seen) {
    assert(b);
  }
}

void test_bot_asks_across_all_held_suits() {
  std::mt19937 rng(13);
  BotTable t;
  t.bots[0]->bankroll = 0;
  for (auto s : {Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs}) {
    for (int k = 0; k < 100; ++k) {
      t.bots[0]->hand.push_back(Card{s});
    }
  }
  std::array<bool, kSuitCount> seen{};
  for (int i = 0; i < 5'000; ++i) {
    t.book = OrderBook{};
    t.act(0, rng);
    for (std::size_t s = 0; s < kSuitCount; ++s) {
      if (!t.book.asks()[s].empty()) {
        seen[s] = true;
      }
    }
  }
  for (auto b : seen) {
    assert(b);
  }
}

// ============================================================================
// 5. OrderBook submission invariants
// ============================================================================

void test_book_rejects_bid_when_bankroll_insufficient() {
  BotTable t;
  t.bots[0]->bankroll = 5;
  const bool ok = t.book.submit_bid(t.table, 0, Suit::Spades, 10);
  assert(ok == false);
  assert(t.book.bids()[static_cast<std::size_t>(Suit::Spades)].empty());
}

void test_book_accepts_bid_at_exact_bankroll() {
  BotTable t;
  t.bots[0]->bankroll = 10;
  // No matching ask, so no immediate trade — submit_bid returns false but the bid does rest.
  const bool ok = t.book.submit_bid(t.table, 0, Suit::Spades, 10);
  assert(ok == false);
  const auto& spades_bids = t.book.bids()[static_cast<std::size_t>(Suit::Spades)];
  assert(spades_bids.size() == 1);
  assert(spades_bids.front().price == 10);
}

void test_book_rejects_nonpositive_prices() {
  BotTable t;
  t.bots[0]->bankroll = 100;
  t.bots[0]->hand = {Card{Suit::Spades}};
  assert(t.book.submit_bid(t.table, 0, Suit::Spades, 0) == false);
  assert(t.book.submit_bid(t.table, 0, Suit::Spades, -5) == false);
  assert(t.book.submit_ask(t.table, 0, Suit::Spades, 0) == false);
  assert(t.book.submit_ask(t.table, 0, Suit::Spades, -5) == false);
  assert(total_orders(t.book.bids()) == 0);
  assert(total_orders(t.book.asks()) == 0);
}

void test_book_rejects_ask_when_seller_lacks_card() {
  BotTable t;
  t.bots[0]->hand = {Card{Suit::Hearts}};  // no spades
  const bool ok = t.book.submit_ask(t.table, 0, Suit::Spades, 10);
  assert(ok == false);
  assert(t.book.asks()[static_cast<std::size_t>(Suit::Spades)].empty());
}

void test_book_rejects_self_trade() {
  BotTable t;
  t.bots[0]->bankroll = 100;
  t.bots[0]->hand = {Card{Suit::Spades}};
  // Player 0 rests an ask, then submits a bid that would cross their own ask.
  assert(t.book.submit_ask(t.table, 0, Suit::Spades, 10) == false);
  assert(t.book.submit_bid(t.table, 0, Suit::Spades, 15) == false);
  // No state change — the self-cross is dropped, not settled.
  assert(t.bots[0]->bankroll == 100);
  assert(t.bots[0]->hand.size() == 1);
}

// ============================================================================
// 6. Trade settlement correctness
// ============================================================================

void test_trade_transfers_card_and_chips() {
  BotTable t;
  t.bots[0]->bankroll = 100;                 // buyer
  t.bots[1]->bankroll = 100;                 // seller
  t.bots[1]->hand = {Card{Suit::Hearts}};

  // Seller rests an ask at 20; buyer crosses with bid at 25 → trade at the ask price (20).
  assert(t.book.submit_ask(t.table, 1, Suit::Hearts, 20) == false);
  assert(t.book.submit_bid(t.table, 0, Suit::Hearts, 25) == true);

  assert(t.bots[0]->bankroll == 80);
  assert(t.bots[1]->bankroll == 120);
  assert(t.bots[1]->hand.empty());
  assert(t.bots[0]->hand.size() == 1);
  assert(t.bots[0]->hand[0].suit == Suit::Hearts);
  assert(t.book.trades().size() == 1);
}

void test_chips_are_conserved_across_a_trade() {
  BotTable t;
  t.bots[0]->bankroll = 100;
  t.bots[1]->bankroll = 50;
  t.bots[1]->hand = {Card{Suit::Diamonds}};

  std::int32_t before = 0;
  for (auto& b : t.bots) {
    before += b->bankroll;
  }
  (void)t.book.submit_ask(t.table, 1, Suit::Diamonds, 30);
  (void)t.book.submit_bid(t.table, 0, Suit::Diamonds, 35);
  std::int32_t after = 0;
  for (auto& b : t.bots) {
    after += b->bankroll;
  }
  assert(before == after);
}

void test_book_clears_after_a_trade() {
  BotTable t;
  for (auto& b : t.bots) {
    b->bankroll = 1'000;
  }
  t.bots[1]->hand = {Card{Suit::Hearts}, Card{Suit::Spades}};
  t.bots[2]->hand = {Card{Suit::Clubs}};

  // Resting orders across multiple suits that don't cross each other.
  (void)t.book.submit_bid(t.table, 0, Suit::Hearts, 5);
  (void)t.book.submit_ask(t.table, 1, Suit::Spades, 30);
  (void)t.book.submit_ask(t.table, 2, Suit::Clubs, 25);

  // A crossing ask on Hearts triggers a trade and the global "clear all" rule.
  (void)t.book.submit_ask(t.table, 1, Suit::Hearts, 4);

  for (std::size_t s = 0; s < kSuitCount; ++s) {
    assert(t.book.bids()[s].empty());
    assert(t.book.asks()[s].empty());
  }
}

// ============================================================================
// 7. Multi-round Game flow
// ============================================================================

void test_game_constructor_antes_each_player() {
  std::mt19937 rng(100);
  CoutSilencer s;
  Game game(rng);
  assert(game.pot() == kInitialPot);
  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    assert(game.players()[i]->bankroll == kStartingBankroll - kAnteAmount);
  }
}

void test_game_keeps_same_player_objects_across_rounds() {
  std::mt19937 rng(101);
  CoutSilencer s;
  Game game(rng);
  std::array<Player*, kPlayerCount> ptrs{};
  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    ptrs[i] = game.players()[i].get();
  }
  for (int r = 0; r + 1 < kRoundsPerGame; ++r) {
    game.run_round(rng);
    game.start_next_round(rng);
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
      assert(game.players()[i].get() == ptrs[i]);
    }
  }
}

void test_game_antes_each_subsequent_round() {
  std::mt19937 rng(102);
  CoutSilencer s;
  Game game(rng);
  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    game.players()[i]->bankroll = 200;
  }
  game.start_next_round(rng);
  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    assert(game.players()[i]->bankroll == 200 - kAnteAmount);
  }
}

void test_game_pot_resets_each_round() {
  std::mt19937 rng(103);
  CoutSilencer s;
  Game game(rng);
  assert(game.pot() == kInitialPot);
  game.run_round(rng);
  // Pot drains to 0 during settlement.
  game.start_next_round(rng);
  assert(game.pot() == kInitialPot);
}

void test_game_deals_full_hands_each_round() {
  std::mt19937 rng(104);
  CoutSilencer s;
  Game game(rng);
  for (int r = 0; r < kRoundsPerGame; ++r) {
    std::size_t total_cards = 0;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
      assert(game.players()[i]->hand.size() == kCardsPerPlayer);
      total_cards += game.players()[i]->hand.size();
    }
    assert(total_cards == kCardsPerPlayer * kPlayerCount);
    if (r + 1 < kRoundsPerGame) {
      game.run_round(rng);
      game.start_next_round(rng);
    }
  }
}

void test_game_fair_values_persist_across_rounds() {
  std::mt19937 rng(105);
  CoutSilencer s;
  Game game(rng);
  game.run_round(rng);
  auto* bot0 = static_cast<RandomBot*>(game.players()[0].get());
  const auto snapshot = bot0->fair_values;
  game.start_next_round(rng);
  for (std::size_t i = 0; i < snapshot.size(); ++i) {
    assert(bot0->fair_values[i] == snapshot[i]);
  }
}

}  // namespace

#define RUN(fn)                                  \
  do {                                           \
    fn();                                        \
    std::cout << "PASS: " #fn << "\n";           \
  } while (0)

int main() {
  // 1. fair_values
  RUN(test_fair_values_initialised_to_15);
  RUN(test_fair_values_stay_within_clamp_bounds);
  RUN(test_fair_values_drift_by_at_most_one_per_act);
  RUN(test_fair_values_actually_drift_over_time);

  // 2. pricing formulas
  RUN(test_bid_price_within_fair_value_minus_two_band);
  RUN(test_ask_price_within_fair_value_plus_two_band);

  // 3. bot guards
  RUN(test_broke_bot_never_submits_a_bid);
  RUN(test_low_bankroll_caps_bid_price);
  RUN(test_empty_hand_bot_never_submits_an_ask);
  RUN(test_bot_only_sells_suits_it_holds);
  RUN(test_idle_bot_does_nothing_when_resourceless);

  // 4. randomness
  RUN(test_bot_takes_all_three_modes_over_many_turns);
  RUN(test_bot_bids_across_all_four_suits);
  RUN(test_bot_asks_across_all_held_suits);

  // 5. order book invariants
  RUN(test_book_rejects_bid_when_bankroll_insufficient);
  RUN(test_book_accepts_bid_at_exact_bankroll);
  RUN(test_book_rejects_nonpositive_prices);
  RUN(test_book_rejects_ask_when_seller_lacks_card);
  RUN(test_book_rejects_self_trade);

  // 6. trade settlement
  RUN(test_trade_transfers_card_and_chips);
  RUN(test_chips_are_conserved_across_a_trade);
  RUN(test_book_clears_after_a_trade);

  // 7. multi-round game flow
  RUN(test_game_constructor_antes_each_player);
  RUN(test_game_keeps_same_player_objects_across_rounds);
  RUN(test_game_antes_each_subsequent_round);
  RUN(test_game_pot_resets_each_round);
  RUN(test_game_deals_full_hands_each_round);
  RUN(test_game_fair_values_persist_across_rounds);

  std::cout << "\nAll tests passed.\n";
  return 0;
}
