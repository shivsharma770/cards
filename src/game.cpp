#include "figgie/game.hpp"

#include "figgie/card.hpp"
#include "figgie/deck.hpp"
#include "figgie/random_bot.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

namespace figgie {

namespace {

[[nodiscard]] Suit find_long_suit(const SuitDistribution& dist) noexcept {
  for (std::size_t i = 0; i < kSuitCount; ++i) {
    if (dist.count_per_suit[i] == kLongSuitCount) {
      return static_cast<Suit>(i);
    }
  }
  return Suit::Spades;  // Invalid distribution; unreachable for generated decks.
}

}  // namespace

Suit Game::compute_goal_suit(const SuitDistribution& dist) noexcept {
  const Suit long_suit = find_long_suit(dist);
  return partner_suit_same_color(long_suit);
}

void Game::initialize_round(std::mt19937& rng) {
  auto [deck, dist] = make_shuffled_deck_with_distribution(rng);

  goal_suit_ = compute_goal_suit(dist);

  pot_ = kInitialPot;

  for (std::size_t p = 0; p < kPlayerCount; ++p) {
    players_[p] = std::make_unique<RandomBot>();
    players_[p]->bankroll = kStartingBankroll - kAnteAmount;
    players_[p]->hand.clear();
    players_[p]->hand.reserve(kCardsPerPlayer);
    for (std::size_t c = 0; c < kCardsPerPlayer; ++c) {
      players_[p]->hand.push_back(deck[p * kCardsPerPlayer + c]);
    }
  }
}

Game::Game(std::mt19937& rng) { initialize_round(rng); }

void Game::run_round(std::mt19937& rng) {
  std::cout << "--- Trading: " << kTicksPerRound << " ticks, " << kPlayerCount << " seats ---\n";
  std::size_t trade_cursor = 0;

  for (int tick = 0; tick < kTicksPerRound; ++tick) {
    std::array<Player*, kPlayerCount> table = player_ptr_table();
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
      players_[i]->act(order_book_, table, i, rng);

      const auto& trades = order_book_.trades();
      while (trade_cursor < trades.size()) {
        const auto& t = trades[trade_cursor++];
        std::cout << "[trade #" << t.sequence << "] tick=" << tick << " suit=" << suit_name(t.suit)
                  << " buyer=" << t.buyer << " seller=" << t.seller << " price=" << t.price << "\n";
      }
    }
  }

  settle_round();
}

void Game::settle_round() {
  std::cout << "\n=== End of round ===\n";
  std::cout << "Goal suit: " << suit_name(goal_suit_) << "\n";

  std::array<std::size_t, kPlayerCount> goal_counts{};
  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    for (const auto& c : players_[i]->hand) {
      if (c.suit == goal_suit_) {
        ++goal_counts[i];
      }
    }
  }

  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    const auto pay =
        static_cast<std::int32_t>(goal_counts[i]) * kGoalCardChipValue;
    pot_ -= pay;
    players_[i]->bankroll += pay;
    std::cout << "Player " << i << ": +" << pay << " chips for " << goal_counts[i]
              << " goal card(s) (pot now " << pot_ << ")\n";
  }

  std::size_t max_goals = 0;
  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    max_goals = std::max(max_goals, goal_counts[i]);
  }

  std::vector<std::size_t> majority{};
  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    if (goal_counts[i] == max_goals) {
      majority.push_back(i);
    }
  }

  const std::int32_t bonus = pot_;
  pot_ = 0;

  if (bonus > 0 && !majority.empty()) {
    const auto n = static_cast<std::int32_t>(majority.size());
    const std::int32_t base_share = bonus / n;
    const std::int32_t remainder = bonus % n;
    std::cout << "Majority bonus (most goal cards): " << bonus << " chips split among "
              << majority.size() << " player(s)\n";
    for (std::int32_t j = 0; j < n; ++j) {
      const std::int32_t extra = (j < remainder) ? 1 : 0;
      const std::int32_t share = base_share + extra;
      players_[majority[static_cast<std::size_t>(j)]]->bankroll += share;
      std::cout << "  Player " << majority[static_cast<std::size_t>(j)] << " gets +" << share
                << " (majority bonus)\n";
    }
  } else {
    std::cout << "Majority bonus: 0 (empty pot or no eligible majority)\n";
  }

  std::cout << "\n--- Final leaderboard (by chips) ---\n";
  std::vector<std::size_t> order(kPlayerCount);
  for (std::size_t i = 0; i < kPlayerCount; ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
    return players_[a]->bankroll > players_[b]->bankroll;
  });

  for (std::size_t rank = 0; rank < kPlayerCount; ++rank) {
    const std::size_t seat = order[rank];
    std::cout << (rank + 1) << ". Player " << seat << ": " << players_[seat]->bankroll
              << " chips, " << goal_counts[seat] << " goal card(s)\n";
  }
}

}  // namespace figgie
