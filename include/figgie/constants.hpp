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
inline constexpr std::int32_t kStartingBankroll = 350;
inline constexpr std::int32_t kAnteAmount = 50;
inline constexpr std::int32_t kInitialPot = kAnteAmount * static_cast<std::int32_t>(kPlayerCount);

/// Simulation / round pacing.
inline constexpr int kTicksPerRound = 200;

/// End-of-round payout per Goal Suit card (paid from the pot).
inline constexpr std::int32_t kGoalCardChipValue = 10;

}  // namespace figgie
