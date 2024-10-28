#pragma once
#include <optional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sstream>
#include "iex_messages.h"
#include <optional>
#include <stdexcept>
#include <vector>

struct BBO {
private:
    double bid_price;
    int bid_size;
    double ask_price;
    int ask_size;

public:
    // Constructor that enforces the bid <= ask constraint
    BBO(double bid_price, int bid_size, double ask_price, int ask_size)
        : bid_price(bid_price), bid_size(bid_size), ask_price(ask_price), ask_size(ask_size) {
        if (bid_price > ask_price) {
            throw std::invalid_argument("bid_price cannot be greater than ask_price");
        }
    }

    // Getters
    double getBidPrice() const { return bid_price; }
    int getBidSize() const { return bid_size; }
    double getAskPrice() const { return ask_price; }
    int getAskSize() const { return ask_size; }

    // Setters with validation
    void setBidPrice(double new_bid_price) {
        if (new_bid_price > ask_price) {
            throw std::invalid_argument("bid_price cannot be greater than ask_price");
        }
        bid_price = new_bid_price;
    }

    void setAskPrice(double new_ask_price) {
        if (new_ask_price < bid_price) {
            throw std::invalid_argument("ask_price cannot be less than bid_price");
        }
        ask_price = new_ask_price;
    }
};


// OrderBook class definition
class OrderBook {
private:
    std::map<double, int> bid_levels; // Price -> Size
    std::map<double, int> ask_levels; // Price -> Size
    std::map<std::string, std::vector<PriceLevelUpdateMessage>> atomicUpdates; // Symbol -> Updates
    std::optional<BBO> current_bbo; // Best Bid and Offer as a class field

public:
    // Constructor and destructor
    OrderBook() = default;
    ~OrderBook() = default;

    // Retrieve the current Best Bid and Offer
    std::optional<BBO> GetBbo() const;

    // Process incoming messages
    void ProcessMessage(std::unique_ptr<IEXMessageBase> message);

    // Update the order book based on the message type
    void UpdateOrderBook(MessageType type, const std::string& symbol, double price, int size);

    // Print the current state of the order book
    void PrintOrderBook() const;

    // Print the Best Bid and Offer
    void PrintBbo() const;
    double GetBookPressure() const;
    // Update the Best Bid and Offer (BBO)
    void UpdateBBO();

private:
    // Start atomic update by adding the message to the atomic update map
    void startAtomicUpdate(PriceLevelUpdateMessage* update);

    // End atomic update and apply all updates for the symbol
    void endAtomicUpdate(PriceLevelUpdateMessage* update);

    // Apply all atomic updates for a symbol, then update the BBO
    void applyAtomicUpdates(const std::string& symbol);


    
};