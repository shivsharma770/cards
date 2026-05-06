#pragma once

#include <cstdint>

namespace figgie {

enum class Suit : std::uint8_t {
  Spades = 0,
  Clubs = 1,
  Hearts = 2,
  Diamonds = 3,
};

enum class Color : std::uint8_t {
  Black = 0,
  Red = 1,
};

/// Cards are distinguished only by suit (ranks are not used in Figgie).
struct Card {
  Suit suit{};
};

[[nodiscard]] constexpr Color color_of(Suit s) noexcept {
  return (s == Suit::Spades || s == Suit::Clubs) ? Color::Black : Color::Red;
}

/// The other suit of the same color (Spades <-> Clubs, Hearts <-> Diamonds).
[[nodiscard]] inline const char* suit_name(Suit s) noexcept {
  switch (s) {
    case Suit::Spades:
      return "Spades";
    case Suit::Clubs:
      return "Clubs";
    case Suit::Hearts:
      return "Hearts";
    case Suit::Diamonds:
      return "Diamonds";
  }
  return "Spades";
}

[[nodiscard]] constexpr Suit partner_suit_same_color(Suit s) noexcept {
  if (s == Suit::Spades) {
    return Suit::Clubs;
  }
  if (s == Suit::Clubs) {
    return Suit::Spades;
  }
  if (s == Suit::Hearts) {
    return Suit::Diamonds;
  }
  return Suit::Hearts;
}

}  // namespace figgie
