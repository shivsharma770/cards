#pragma once

#include "figgie/card.hpp"
#include "figgie/constants.hpp"
#include "figgie/player.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace figgie {

/// Central limit order book: one bid stack and one ask stack per suit.
/// Per official Figgie rule modeled here: after any fill, **all** resting bids and asks
/// on **every** suit are cleared.
class OrderBook {
 public:
  struct Order {
    std::uint64_t id{};
    std::size_t player{};
    Suit suit{};
    std::int32_t price{};
  };

  struct Trade {
    std::uint64_t sequence{};
    Suit suit{};
    std::size_t buyer{};
    std::size_t seller{};
    std::int32_t price{};
    std::uint64_t buyer_order_id{};
    std::uint64_t seller_order_id{};
  };

  /// Resting bid. Returns `true` if an immediate trade occurred (book fully cleared after).
  [[nodiscard]] bool submit_bid(std::array<Player*, kPlayerCount>& players, std::size_t buyer_id,
                                Suit suit, std::int32_t price);

  /// Resting ask / offer. Returns `true` if an immediate trade occurred (book fully cleared after).
  [[nodiscard]] bool submit_ask(std::array<Player*, kPlayerCount>& players, std::size_t seller_id,
                                Suit suit, std::int32_t price);

  [[nodiscard]] const std::array<std::vector<Order>, kSuitCount>& bids() const noexcept {
    return bids_;
  }

  [[nodiscard]] const std::array<std::vector<Order>, kSuitCount>& asks() const noexcept {
    return asks_;
  }

  [[nodiscard]] const std::vector<Trade>& trades() const noexcept { return trades_; }

  /// Mid-market signal for `suit`: average of best bid and best ask. Returns
  /// `-1.0f` if either side of the book is empty (i.e. there is no usable
  /// signal — caller should not feed -1 into a price model).
  [[nodiscard]] float get_mid_price(Suit suit) const noexcept;

 private:
  void clear_entire_book() noexcept;

  [[nodiscard]] bool try_settle_trade(std::array<Player*, kPlayerCount>& players, Suit suit,
                                      std::size_t buyer, std::size_t seller, std::int32_t price,
                                      std::uint64_t buyer_order_id, std::uint64_t seller_order_id);

  static void insert_bid_sorted(std::vector<Order>& book, Order o);
  static void insert_ask_sorted(std::vector<Order>& book, Order o);

  std::array<std::vector<Order>, kSuitCount> bids_{};
  std::array<std::vector<Order>, kSuitCount> asks_{};
  std::vector<Trade> trades_{};
  std::uint64_t next_order_id_{1};
  std::uint64_t next_trade_sequence_{1};
};

}  // namespace figgie
