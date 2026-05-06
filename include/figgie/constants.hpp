#pragma once

#include <cstddef>
#include <cstdint>

namespace figgie {

/// Total cards in a Figgie deck.
inline constexpr std::size_t kDeckSize = 40;

/// Number of distinct suits.
inline constexpr std::size_t kSuitCount = 4;

/// Suit histogram for one game: one suit has 12, two have 10, one has 8.
inline constexpr std::uint8_t kLongSuitCount = 12;
inline constexpr std::uint8_t kMediumSuitCount = 10;
inline constexpr std::uint8_t kShortSuitCount = 8;

/// One table; each player is dealt an equal share of the 40-card deck.
inline constexpr std::size_t kPlayerCount = 4;
inline constexpr std::size_t kCardsPerPlayer = kDeckSize / kPlayerCount;

/// Money (toy currency units per Figgie rules of this project).
/// Inflated to absorb HFT-scale variance from the random-walk bots without
/// going bust early in a session.
inline constexpr std::int32_t kStartingBankroll = 5000;
inline constexpr std::int32_t kAnteAmount = 50;
inline constexpr std::int32_t kInitialPot = kAnteAmount * static_cast<std::int32_t>(kPlayerCount);

/// Simulation / round pacing.
inline constexpr int kTicksPerRound = 200;
inline constexpr int kRoundsPerGame = 5;

/// Visual pacing for a "live ticker" demo when watching trades scroll past.
///   0  -> HFT mode: plain output, no slowdown.
///   >0 -> demo mode: ANSI-coloured suit glyphs, flushed and paced by this many
///         milliseconds after each trade. ~30-60 looks good in a terminal.
inline constexpr int kTradeDisplayDelayMs = 0;

/// End-of-round payout per Goal Suit card (paid from the pot).
inline constexpr std::int32_t kGoalCardChipValue = 10;

}  // namespace figgie
