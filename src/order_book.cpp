#include "figgie/order_book.hpp"

#include <algorithm>

namespace figgie {

namespace {

[[nodiscard]] bool remove_one_card_of_suit(Player& p, Suit s) noexcept {
  for (auto it = p.hand.begin(); it != p.hand.end(); ++it) {
    if (it->suit == s) {
      p.hand.erase(it);
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool player_has_card(const Player& p, Suit s) noexcept {
  for (const auto& c : p.hand) {
    if (c.suit == s) {
      return true;
    }
  }
  return false;
}

}  // namespace

void OrderBook::clear_entire_book() noexcept {
  for (auto& b : bids_) {
    b.clear();
  }
  for (auto& a : asks_) {
    a.clear();
  }
}

void OrderBook::insert_bid_sorted(std::vector<Order>& book, Order o) {
  book.push_back(o);
  std::sort(book.begin(), book.end(), [](const Order& a, const Order& b) {
    if (a.price != b.price) {
      return a.price > b.price;
    }
    return a.id < b.id;
  });
}

void OrderBook::insert_ask_sorted(std::vector<Order>& book, Order o) {
  book.push_back(o);
  std::sort(book.begin(), book.end(), [](const Order& a, const Order& b) {
    if (a.price != b.price) {
      return a.price < b.price;
    }
    return a.id < b.id;
  });
}

bool OrderBook::try_settle_trade(std::array<Player*, kPlayerCount>& players, Suit suit,
                                 std::size_t buyer, std::size_t seller, std::int32_t price,
                                 std::uint64_t buyer_order_id, std::uint64_t seller_order_id) {
  if (buyer >= kPlayerCount || seller >= kPlayerCount) {
    return false;
  }
  if (buyer == seller) {
    return false;
  }
  if (price <= 0) {
    return false;
  }

  Player* buyer_p = players[buyer];
  Player* seller_p = players[seller];
  if (buyer_p == nullptr || seller_p == nullptr) {
    return false;
  }
  if (buyer_p->bankroll < price) {
    return false;
  }
  if (!remove_one_card_of_suit(*seller_p, suit)) {
    return false;
  }

  buyer_p->bankroll -= price;
  seller_p->bankroll += price;
  buyer_p->hand.push_back(Card{suit});

  trades_.push_back(Trade{next_trade_sequence_++, suit, buyer, seller, price, buyer_order_id,
                          seller_order_id});
  return true;
}

bool OrderBook::submit_bid(std::array<Player*, kPlayerCount>& players, std::size_t buyer_id, Suit suit,
                           std::int32_t price) {
  if (buyer_id >= kPlayerCount || price <= 0) {
    return false;
  }
  if (players[buyer_id] == nullptr || players[buyer_id]->bankroll < price) {
    return false;
  }

  const auto suit_i = static_cast<std::size_t>(suit);
  Order incoming{.id = next_order_id_++, .player = buyer_id, .suit = suit, .price = price};

  auto& ask_book = asks_[suit_i];
  if (!ask_book.empty() && incoming.price >= ask_book.front().price) {
    const Order& best_ask = ask_book.front();
    // Maker is the resting ask: trade at the ask price.
    if (try_settle_trade(players, suit, incoming.player, best_ask.player, best_ask.price, incoming.id,
                         best_ask.id)) {
      clear_entire_book();
      return true;
    }
    return false;
  }

  insert_bid_sorted(bids_[suit_i], incoming);
  return false;
}

bool OrderBook::submit_ask(std::array<Player*, kPlayerCount>& players, std::size_t seller_id, Suit suit,
                           std::int32_t price) {
  if (seller_id >= kPlayerCount || price <= 0) {
    return false;
  }
  if (players[seller_id] == nullptr || !player_has_card(*players[seller_id], suit)) {
    return false;
  }

  const auto suit_i = static_cast<std::size_t>(suit);
  Order incoming{.id = next_order_id_++, .player = seller_id, .suit = suit, .price = price};

  auto& bid_book = bids_[suit_i];
  if (!bid_book.empty() && bid_book.front().price >= incoming.price) {
    const Order& best_bid = bid_book.front();
    // Maker is the resting bid: trade at the bid price.
    if (try_settle_trade(players, suit, best_bid.player, incoming.player, best_bid.price, best_bid.id,
                         incoming.id)) {
      clear_entire_book();
      return true;
    }
    return false;
  }

  insert_ask_sorted(asks_[suit_i], incoming);
  return false;
}

}  // namespace figgie
