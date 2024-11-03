

#include <unordered_map>
#include <vector>
#include <iostream>
#include <memory>
#include <algorithm>
#include <string>
#include <set>
#include "iex_messages.h" // Include the necessary headers for message types
#include "order.h"
#include "l3book.h"


size_t L3OrderBook::GetPriceLevelIndex(double price) const {
    if (price < min_price || price > max_price) {
        throw std::out_of_range("Price out of range");
    }
    return static_cast<size_t>((price - min_price) / price_increment);
}

void L3OrderBook::ProcessMessage(const IEXMessageBase& message) {
    switch (message.GetMessageType()) {
        case MessageType::AddOrder: {
            auto* add_order = dynamic_cast<const AddOrderMessage*>(&message);
            if (add_order) {
                AddOrder(*add_order);
            } else {
                std::cerr << "Failed to cast to AddOrderMessage" << std::endl;
            }
            break;
        }
        case MessageType::OrderModify: {
            auto* modify_order = dynamic_cast<const OrderModifyMessage*>(&message);
            if (modify_order) {
                ModifyOrder(*modify_order);
            } else {
                std::cerr << "Failed to cast to OrderModifyMessage" << std::endl;
            }
            break;
        }
        case MessageType::OrderDelete: {
            auto* delete_order = dynamic_cast<const OrderDeleteMessage*>(&message);
            if (delete_order) {
                DeleteOrder(*delete_order);
            } else {
                std::cerr << "Failed to cast to OrderDeleteMessage" << std::endl;
            }
            break;
        }
        case MessageType::OrderExecuted: {
            auto* executed_order = dynamic_cast<const OrderExecutedMessage*>(&message);
            if (executed_order) {
                ExecuteOrder(*executed_order);
            } else {
                std::cerr << "Failed to cast to OrderExecutedMessage" << std::endl;
            }
            break;
        }
        case MessageType::TradeReport: {
            auto* trade_message = dynamic_cast<const TradeReportMessage*>(&message);
            if (trade_message) {
                HandleTrade(*trade_message);
            } else {
                std::cerr << "Failed to cast to TradeReportMessage" << std::endl;
            }
            break;
        }
        default:
            std::cerr << "Unknown message type" << std::endl;
            break;
    }
}

void L3OrderBook::AddOrder(const AddOrderMessage& message) {
    // Create a new order based on the incoming message
    Order new_order = { message.order_id, message.size, message.price, message.side };

    // Insert the new order into the global orders map
    orders[message.order_id] = new_order;

    // Determine the price level index
    size_t index = GetPriceLevelIndex(new_order.price);
    price_levels[index].push_back(new_order); // Insert into the corresponding price level

    std::cout << "Added Order: ID: " << (int)new_order.order_id 
              << ", Size: " << (int)new_order.size 
              << ", Price: " << new_order.price 
              << ", Side: " << (new_order.side == Side::Buy ? "Buy" : "Sell") 
              << std::endl;
}

void L3OrderBook::ModifyOrder(const OrderModifyMessage& message) {
    auto it = orders.find(message.order_id_ref);
    if (it != orders.end()) {
        Order& order = it->second;

        // Remove the existing order from the appropriate price level
        size_t index = GetPriceLevelIndex(order.price);
        auto& level_orders = price_levels[index];
        level_orders.erase(std::remove_if(level_orders.begin(), level_orders.end(),
                                           [&order](const Order& o) { return o.order_id == order.order_id; }),
                           level_orders.end());

        // Modify the order properties (size, price)
        order.size = message.size;
        order.price = message.price;

        // Reinsert the modified order to the new price level
        size_t new_index = GetPriceLevelIndex(order.price);
        price_levels[new_index].push_back(order);

        std::cout << "Modified Order: ID: " << (int)order.order_id
                  << ", Size: " << (int)order.size 
                  << ", Price: " << order.price 
                  << std::endl;

    } else {
        std::cerr << "Order ID: " << (int)message.order_id_ref 
                  << " not found for modification." << std::endl;
    }
}

void L3OrderBook::DeleteOrder(const OrderDeleteMessage& message) {
    auto it = orders.find(message.order_id_ref);
    if (it != orders.end()) {
        const Order& order = it->second;

        // Remove the order from the corresponding price level
        size_t index = GetPriceLevelIndex(order.price);
        auto& level_orders = price_levels[index];
        level_orders.erase(std::remove_if(level_orders.begin(), level_orders.end(),
                                           [&order](const Order& o) { return o.order_id == order.order_id; }),
                           level_orders.end());

        // Now remove the order from the global orders map
        orders.erase(it);
        std::cout << "Removed Order ID: " << (int)message.order_id_ref << " from orders." << std::endl;
    } else {
        std::cerr << "Order ID: " << (int)message.order_id_ref << " not found for removal." << std::endl;
    }
}

void L3OrderBook::ExecuteOrder(const OrderExecutedMessage& message) {
    // Find the order by order_id_ref
    auto it = orders.find(message.order_id_ref);
    if (it != orders.end()) {
        Order& order = it->second;

        // Check if the order is of the right side
        if ((order.side == Side::Buy && message.price < order.price) || 
            (order.side == Side::Sell && message.price > order.price)) {
            std::cerr << "Executed price does not match order type. Cannot execute." << std::endl;
            return; // Ignore the execution if the price does not match the order side
        }

        // Calculate the remaining quantity
        uint8_t remaining_size = order.size - message.size;

        if (remaining_size == 0) {
            // If quantity reduces to zero, remove the order from the order book
            size_t index = GetPriceLevelIndex(order.price);
            auto& level_orders = price_levels[index];
            level_orders.erase(std::remove_if(level_orders.begin(), level_orders.end(),
                                               [&order](const Order& o) { return o.order_id == order.order_id; }),
                               level_orders.end());

            // Remove the order from the global orders map
            orders.erase(it);
            std::cout << "Order ID: " << (int)message.order_id_ref << " executed completely and removed from the order book." << std::endl;
        } else {
            // Update the order's remaining quantity but keep it in the order book
            order.size = remaining_size;
            std::cout << "Order ID: " << (int)message.order_id_ref << " executed partially. Remaining size: " 
                      << (int)remaining_size << std::endl;
        }
    } else {
        std::cerr << "Order ID: " << (int)message.order_id_ref << " not found for execution." << std::endl;
    }
}

void L3OrderBook::HandleTrade(const TradeReportMessage& message) {
    std::cout << "Trade executed: ID: " << (int)message.trade_id 
              << ", Price: " << message.price 
              << ", Size: " << (int)message.size 
              << std::endl;

    // Check if the trade price and size are valid
    if (message.size == 0) {
        std::cerr << "Trade size is zero. Ignoring trade." << std::endl;
        return; // Ignore if the trade size is zero
    }

    // We need to adjust the corresponding buy or sell orders in the order book
    // Depending on the trade price, we will process it against the corresponding side of the order book
    // In a real-world scenario, you would also need to match this with existing orders,
    // but for this example, we'll simplify the assumption that the trade is matched.

    // Find orders that match the trade
    for (auto& level_orders : price_levels) {
        for (auto& order : level_orders) {
            // Only process orders that are still active
            if (orders.find(order.order_id) != orders.end()) {
                // Match against buy orders
                if (order.side == Side::Buy && message.price >= order.price) {
                    // Check if we can fill the order
                    if (order.size >= message.size) {
                        // Partially or completely fill the order
                        order.size -= message.size;

                        std::cout << "Matched Buy Order ID: " << (int)order.order_id 
                                  << ", Remaining Size: " << (int)order.size << std::endl;

                        // If the order is completely filled, remove it from the order book
                        if (order.size == 0) {
                            size_t index = GetPriceLevelIndex(order.price);
                            auto& level_orders = price_levels[index];
                            level_orders.erase(std::remove_if(level_orders.begin(), level_orders.end(),
                                                               [&order](const Order& o) { return o.order_id == order.order_id; }),
                                               level_orders.end());

                            // Remove from global orders map
                            orders.erase(order.order_id);
                            std::cout << "Buy Order ID: " << (int)order.order_id 
                                      << " completely filled and removed from the order book." << std::endl;
                        }
                        return; // Exit after processing the trade
                    }
                }
                // Match against sell orders
                else if (order.side == Side::Sell && message.price <= order.price) {
                    // Check if we can fill the order
                    if (order.size >= message.size) {
                        // Partially or completely fill the order
                        order.size -= message.size;

                        std::cout << "Matched Sell Order ID: " << (int)order.order_id 
                                  << ", Remaining Size: " << (int)order.size << std::endl;

                        // If the order is completely filled, remove it from the order book
                        if (order.size == 0) {
                            size_t index = GetPriceLevelIndex(order.price);
                            auto& level_orders = price_levels[index];
                            level_orders.erase(std::remove_if(level_orders.begin(), level_orders.end(),
                                                               [&order](const Order& o) { return o.order_id == order.order_id; }),
                                               level_orders.end());

                            // Remove from global orders map
                            orders.erase(order.order_id);
                            std::cout << "Sell Order ID: " << (int)order.order_id 
                                      << " completely filled and removed from the order book." << std::endl;
                        }
                        return; // Exit after processing the trade
                    }
                }
            }
        }
    }

    std::cerr << "No matching order found for Trade ID: " << (int)message.trade_id << std::endl;
}

void L3OrderBook::PrintOrderBook() const {
    std::cout << "Current Order Book:" << std::endl;
    for (size_t i = 0; i < price_levels.size(); ++i) {
        if (!price_levels[i].empty()) {
            std::cout << "Price Level " << (min_price + i * price_increment) << ": ";
            for (const auto& order : price_levels[i]) {
                std::cout << "[ID: " << (int)order.order_id << ", Size: " << (int)order.size 
                          << ", Side: " << (order.side == Side::Buy ? "Buy" : "Sell") << "] ";
            }
            std::cout << std::endl;
        }
    }
}
