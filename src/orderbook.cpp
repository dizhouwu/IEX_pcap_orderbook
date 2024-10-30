
#include "iex_decoder.h"
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sstream>
#include <orderbook.h>


// Retrieve the current Best Bid and Offer
std::optional<BBO> OrderBook::GetBbo() const {
    return current_bbo;
}

// Process incoming messages
void OrderBook::ProcessMessage(const IEXMessageBase& message) {
    auto message_type = message.GetMessageType();

    if (message_type == MessageType::PriceLevelUpdateBuy || message_type == MessageType::PriceLevelUpdateSell) {
        // Perform a dynamic cast to safely interpret the message as PriceLevelUpdateMessage
        auto* price_level_update = dynamic_cast<const PriceLevelUpdateMessage*>(&message);

        if (price_level_update) {
            if (price_level_update->flags == 0) {
                // Start of an atomic event
                startAtomicUpdate(price_level_update);
            } else {
                // End of a transaction
                if (atomicUpdates.find(price_level_update->symbol) != atomicUpdates.end()) {
                    endAtomicUpdate(price_level_update);
                } else {
                    // No atomic update, process directly
                    UpdateOrderBook(
                        price_level_update->GetMessageType(),
                        price_level_update->symbol,
                        price_level_update->price,
                        price_level_update->size
                    );
                    price_level_update->Print();
                    std::cout << "update BBO when it is 1 level take out" << std::endl;
                    UpdateBBO(); // Update BBO immediately for non-atomic updates
                }
            }
        }
    }
}


// Update the order book based on the message type
void OrderBook::UpdateOrderBook(MessageType type, const std::string& symbol, double price, int size) {
    if (type == MessageType::PriceLevelUpdateBuy) {
        if (size == 0) {
            bid_levels.erase(price); // Remove price level if size is zero
        } else {
            bid_levels[price] = size; // Update bid level
        }
    } else if (type == MessageType::PriceLevelUpdateSell) {
        if (size == 0) {
            ask_levels.erase(price); // Remove price level if size is zero
        } else {
            ask_levels[price] = size; // Update ask level
        }
    }
}

void OrderBook::PrintOrderBook() const {
    std::cout << "Bids:" << std::endl;
    for (auto it = bid_levels.rbegin(); it != bid_levels.rend(); ++it) {
        std::cout << "Price: " << it->first << ", Size: " << it->second << "\n";
    }

    std::cout << "Asks:" << std::endl;
    for (auto it = ask_levels.begin(); it != ask_levels.end(); ++it) {
        std::cout << "Price: " << it->first << ", Size: " << it->second << "\n";
    }
    std::cout << std::endl;
}

// Print the Best Bid and Offer
void OrderBook::PrintBbo() const {
    if (current_bbo) {
        std::cout << "Best Bid: Price = " << current_bbo -> getBidPrice() << ", Size = " << current_bbo -> getBidSize()<< "\n";
        std::cout << "Best Ask: Price = " << current_bbo->getAskPrice() << ", Size = " << current_bbo->getAskSize() << "\n";
    } else {
        std::cout << "No Best Bid or Offer available.\n";
    }
}

// Start atomic update by adding the message to the atomic update map
void OrderBook::startAtomicUpdate(const PriceLevelUpdateMessage* update) {
    atomicUpdates[update->symbol].push_back(*update);
}

// End atomic update and apply all updates for the symbol
void OrderBook::endAtomicUpdate(const PriceLevelUpdateMessage* update) {
    auto& updates = atomicUpdates[update->symbol];
    updates.push_back(*update);
    applyAtomicUpdates(update->symbol);
    atomicUpdates.erase(update->symbol);
}

// Apply all atomic updates for a symbol, then update the BBO
void OrderBook::applyAtomicUpdates(const std::string& symbol) {
    auto& updates = atomicUpdates[symbol];
    for (const auto& update : updates) {
        UpdateOrderBook(update.GetMessageType(), update.symbol, update.price, update.size);
    }
    std::cout << "updating bbo after applying" << std::endl;
    UpdateBBO(); // Update BBO after atomic updates are applied
}

// Update the Best Bid and Offer (BBO)
void OrderBook::UpdateBBO() {
    if (!bid_levels.empty() && !ask_levels.empty()) {
        auto best_bid = *bid_levels.rbegin(); // Highest bid
        auto best_ask = *ask_levels.begin();   // Lowest ask

        // Debug outputs
        std::cout << "Updating BBO..." << std::endl;
        std::cout << "Best Bid: " << best_bid.first << " (Size: " << best_bid.second << ")" << std::endl;
        std::cout << "Best Ask: " << best_ask.first << " (Size: " << best_ask.second << ")" << std::endl;
        for (auto x: ask_levels){
            std::cout  << "ask price: " <<  x.first << " ask qty" << x.second<<std::endl;
        }
        // Create a new BBO instance
        current_bbo = BBO{
            best_bid.first, // Bid price
            best_bid.second, // Bid size
            best_ask.first,  // Ask price
            best_ask.second  // Ask size
        };
    } else {
        std::cout << "Resetting current BBO: No Bids or Asks available." << std::endl;
        current_bbo.reset(); // No BBO available if either bid or ask levels are empty
    }
}

double OrderBook::GetBookPressure() const {
    int buy_pressure = 0;
    int sell_pressure = 0;

    // Accumulate sizes for the top 5 buy levels
    int count = 0;
    for (auto it = bid_levels.rbegin(); it != bid_levels.rend() && count < 5; ++it, ++count) {
        buy_pressure += it -> second;
    }
    count = 0;
    for (auto it = ask_levels.begin(); it!=ask_levels.end() &&count<5; ++it,++count)
    {
        sell_pressure += it -> second;
    }

    // Calculate the book pressure ratio
    int total_pressure = buy_pressure + sell_pressure;
    if (total_pressure == 0) return 0.0; // Avoid division by zero

    return static_cast<double>(buy_pressure - sell_pressure) / total_pressure;
}

