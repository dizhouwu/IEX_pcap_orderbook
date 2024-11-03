#pragma once

#include <unordered_map>
#include <vector>
#include <iostream>
#include <memory>
#include <algorithm>
#include <string>
#include <set>
#include <stdexcept> // For std::invalid_argument
#include "iex_messages.h" // Include the necessary headers for message types
#include "order.h"

class L3OrderBook {
public:
    // Default constructor
    L3OrderBook()
        : min_price(0.0), max_price(0.0), price_increment(0.0) {
        // Initialize price_levels to an empty state; no price levels set
        price_levels.clear();
    }

    // Parameterized constructor
    L3OrderBook(size_t numPriceLevels, double minPrice, double maxPrice, double priceIncrement)
        : min_price(minPrice), max_price(maxPrice), price_increment(priceIncrement) {
        
        // Ensure that the price increment is valid (non-zero)
        if (priceIncrement <= 0) {
            throw std::invalid_argument("Price increment must be greater than zero.");
        }

        // Resize the price_levels vector to hold the specified number of price levels
        price_levels.resize(numPriceLevels);

        // Populate the price levels
        for (size_t i = 0; i < numPriceLevels; ++i) {
            double price = min_price + i * price_increment;
            // Check if the price exceeds the maximum price
            if (price > max_price) {
                break; // No more valid price levels
            }
            // Add a new vector for this price level
            price_levels[i] = std::vector<Order>(); // Initialize with an empty vector of Orders
        }
    }

    // Public interface to process incoming messages
    void ProcessMessage(const IEXMessageBase& message);
    void PrintOrderBook() const;

private:
    std::unordered_map<uint8_t, Order> orders; // Map to store orders by order_id
    std::vector<std::vector<Order>> price_levels; // Fixed price levels
    double min_price;
    double max_price;
    double price_increment;

    // Private helper functions for processing different message types
    void AddOrder(const AddOrderMessage& message);
    void ModifyOrder(const OrderModifyMessage& message);
    void DeleteOrder(const OrderDeleteMessage& message);
    void ExecuteOrder(const OrderExecutedMessage& message);
    void HandleTrade(const TradeReportMessage& message);
    size_t GetPriceLevelIndex(double price) const;
};
