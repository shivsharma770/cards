#include "figgie/card.hpp"
#include "figgie/constants.hpp"
#include "figgie/deck.hpp"
#include "figgie/market_maker.hpp"
#include "figgie/order_book.hpp"
#include "figgie/player.hpp"
#include "figgie/random_bot.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <vector>

namespace {

constexpr int kBacktestGames = 100'000;
constexpr int kTicksPerGame = 1'000;
constexpr std::int32_t kBacktestStartingBankroll = 5'000;

// Seats [0, kNumMms) are MarketMakers; the rest are RandomBots.
// 1 MM vs 3 RandomBots is the canonical setup.
constexpr std::size_t kNumMms = 1;
static_assert(kNumMms < figgie::kPlayerCount, "Need at least one non-MM seat");

[[nodiscard]] figgie::Suit goal_suit_from_distribution(const figgie::SuitDistribution& dist) noexcept {
  for (std::size_t i = 0; i < figgie::kSuitCount; ++i) {
    if (dist.count_per_suit[i] == figgie::kLongSuitCount) {
      return figgie::partner_suit_same_color(static_cast<figgie::Suit>(i));
    }
  }
  return figgie::Suit::Spades;
}

struct GameResult {
  std::array<std::int32_t, kNumMms> mm_pnls{};
  std::array<std::int32_t, kNumMms> mm_final_bankrolls{};
  std::array<std::int32_t, kNumMms> mm_goal_cards{};
  figgie::Suit goal_suit{};
};

GameResult run_one_game(std::mt19937& rng) {
  std::array<std::unique_ptr<figgie::Player>, figgie::kPlayerCount> players;
  for (std::size_t i = 0; i < figgie::kPlayerCount; ++i) {
    if (i < kNumMms) {
      players[i] = std::make_unique<figgie::MarketMaker>();
    } else {
      players[i] = std::make_unique<figgie::RandomBot>();
    }
  }
  for (auto& p : players) {
    p->bankroll = kBacktestStartingBankroll - figgie::kAnteAmount;
  }

  auto [deck, dist] = figgie::make_shuffled_deck_with_distribution(rng);
  const figgie::Suit goal_suit = goal_suit_from_distribution(dist);

  for (std::size_t p = 0; p < figgie::kPlayerCount; ++p) {
    players[p]->hand.clear();
    players[p]->hand.reserve(figgie::kCardsPerPlayer);
    for (std::size_t c = 0; c < figgie::kCardsPerPlayer; ++c) {
      players[p]->hand.push_back(deck[p * figgie::kCardsPerPlayer + c]);
    }
  }

  figgie::OrderBook book;
  std::array<figgie::Player*, figgie::kPlayerCount> table{};
  for (std::size_t i = 0; i < figgie::kPlayerCount; ++i) {
    table[i] = players[i].get();
  }

  for (int tick = 0; tick < kTicksPerGame; ++tick) {
    for (std::size_t i = 0; i < figgie::kPlayerCount; ++i) {
      players[i]->act(book, table, i, rng);
    }
  }

  GameResult result{};
  result.goal_suit = goal_suit;
  for (std::size_t i = 0; i < kNumMms; ++i) {
    const auto& mm = *players[i];
    std::int32_t goal_cards = 0;
    for (const auto& c : mm.hand) {
      if (c.suit == goal_suit) {
        ++goal_cards;
      }
    }
    // P&L: final bankroll - starting bankroll + paper value of held goal cards.
    // Round is intentionally NOT settled, so pot/majority bonus are excluded;
    // we measure trading P&L plus inventory value only.
    result.mm_pnls[i] =
        mm.bankroll - kBacktestStartingBankroll + goal_cards * figgie::kGoalCardChipValue;
    result.mm_final_bankrolls[i] = mm.bankroll;
    result.mm_goal_cards[i] = goal_cards;
  }
  return result;
}

}  // namespace

int main() {
  using clock = std::chrono::steady_clock;

  std::random_device rd;
  std::mt19937 rng(rd());

  std::cout << "Running " << kBacktestGames << "-game backtest, " << kTicksPerGame
            << " ticks per game (" << kNumMms << " MarketMakers vs "
            << (figgie::kPlayerCount - kNumMms) << " RandomBots)...\n\n";

  std::vector<GameResult> results;
  results.reserve(kBacktestGames);

  // Pooled stats across both MM seats (one sample per MM seat per game).
  std::int64_t total_pnl = 0;
  int wins = 0;
  int losses = 0;
  int ties = 0;
  std::int32_t best_pnl = std::numeric_limits<std::int32_t>::min();
  std::int32_t worst_pnl = std::numeric_limits<std::int32_t>::max();

  // Per-seat breakdown so we can sanity-check that both MM seats are symmetric.
  std::array<std::int64_t, kNumMms> per_seat_total{};
  std::array<int, kNumMms> per_seat_wins{};

  const int progress_step = std::max(1, kBacktestGames / 100);

  const auto start = clock::now();

  for (int g = 0; g < kBacktestGames; ++g) {
    const GameResult r = run_one_game(rng);
    results.push_back(r);
    for (std::size_t i = 0; i < kNumMms; ++i) {
      const auto pnl = r.mm_pnls[i];
      total_pnl += pnl;
      per_seat_total[i] += pnl;
      if (pnl > 0) {
        ++wins;
        ++per_seat_wins[i];
      } else if (pnl < 0) {
        ++losses;
      } else {
        ++ties;
      }
      best_pnl = std::max(best_pnl, pnl);
      worst_pnl = std::min(worst_pnl, pnl);
    }

    if ((g + 1) % progress_step == 0 || g + 1 == kBacktestGames) {
      const std::int64_t samples = static_cast<std::int64_t>(g + 1) * kNumMms;
      const double running_avg = static_cast<double>(total_pnl) / samples;
      const double running_win_rate = 100.0 * wins / samples;
      std::cout << "Game " << std::setw(7) << (g + 1)
                << "  |  running avg P&L = " << std::showpos << std::setprecision(2)
                << std::fixed << running_avg << std::noshowpos
                << "  win rate = " << running_win_rate << "%\n";
    }
  }

  const auto elapsed_secs =
      std::chrono::duration<double>(clock::now() - start).count();

  // Distribution stats: pool every MM-seat result into a single sorted array.
  const std::size_t total_samples =
      static_cast<std::size_t>(kBacktestGames) * kNumMms;
  std::vector<std::int32_t> pnls;
  pnls.reserve(total_samples);
  for (const auto& r : results) {
    for (std::size_t i = 0; i < kNumMms; ++i) {
      pnls.push_back(r.mm_pnls[i]);
    }
  }
  std::sort(pnls.begin(), pnls.end());

  const double avg_pnl = static_cast<double>(total_pnl) / total_samples;
  const double win_rate = 100.0 * wins / total_samples;

  double variance_sum = 0.0;
  for (const auto p : pnls) {
    const double d = static_cast<double>(p) - avg_pnl;
    variance_sum += d * d;
  }
  const double std_dev = std::sqrt(variance_sum / total_samples);

  const auto pct = [&](double q) {
    const auto idx = static_cast<std::size_t>(q * (pnls.size() - 1));
    return pnls[idx];
  };
  const std::int32_t median = pct(0.50);
  const std::int32_t p10 = pct(0.10);
  const std::int32_t p90 = pct(0.90);
  const std::int32_t p25 = pct(0.25);
  const std::int32_t p75 = pct(0.75);

  std::cout << "\n========== " << kBacktestGames << "-Game Backtest Summary ("
            << kNumMms << " MMs vs " << (figgie::kPlayerCount - kNumMms)
            << " RBs) ==========\n";
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Games played:    " << kBacktestGames << "\n";
  std::cout << "MM samples:      " << total_samples << "  (" << kNumMms
            << " seats x " << kBacktestGames << " games)\n";
  std::cout << "Elapsed:         " << elapsed_secs << " s  ("
            << (1000.0 * elapsed_secs / kBacktestGames) << " ms/game)\n";
  std::cout << "Wins:            " << wins << "  (" << std::setprecision(1)
            << win_rate << "%)\n";
  std::cout << "Losses:          " << losses << "  ("
            << (100.0 * losses / total_samples) << "%)\n";
  std::cout << "Breakeven:       " << ties << "  ("
            << (100.0 * ties / total_samples) << "%)\n";
  std::cout << std::setprecision(2);
  std::cout << "Total P&L:       " << std::showpos << total_pnl << std::noshowpos
            << " chips\n";
  std::cout << "Average P&L:     " << std::showpos << avg_pnl << std::noshowpos
            << " chips/MM-game\n";
  std::cout << "Std deviation:   " << std_dev << " chips\n";
  std::cout << "Best MM-game:    " << std::showpos << best_pnl << std::noshowpos << " chips\n";
  std::cout << "Worst MM-game:   " << std::showpos << worst_pnl << std::noshowpos << " chips\n";
  std::cout << "Distribution:    p10=" << std::showpos << p10
            << "  p25=" << p25
            << "  median=" << median
            << "  p75=" << p75
            << "  p90=" << p90 << std::noshowpos << "\n";

  std::cout << "\n--- Per-seat sanity check ---\n";
  for (std::size_t i = 0; i < kNumMms; ++i) {
    const double seat_avg = static_cast<double>(per_seat_total[i]) / kBacktestGames;
    const double seat_win_rate = 100.0 * per_seat_wins[i] / kBacktestGames;
    std::cout << "Seat " << i << ": avg P&L = " << std::showpos
              << std::setprecision(2) << seat_avg << std::noshowpos
              << "  win rate = " << std::setprecision(1) << seat_win_rate << "%\n";
  }

  return 0;
}
