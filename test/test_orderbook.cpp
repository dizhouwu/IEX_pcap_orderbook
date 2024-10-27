

#include "gtest/gtest.h"
#include "orderbook.h"
#include "iex_messages.h"
// Unit test for the OrderBook class
TEST(OrderBookTest, BboTransitionTest) {
    OrderBook order_book;

    // Initialize the Order Book
    order_book.UpdateOrderBook(MessageType::PriceLevelUpdateSell, "ZIEXT", 25.30, 100);
    order_book.UpdateOrderBook(MessageType::PriceLevelUpdateSell, "ZIEXT", 25.20, 100);
    order_book.UpdateOrderBook(MessageType::PriceLevelUpdateSell, "ZIEXT", 25.10, 100);
    order_book.UpdateOrderBook(MessageType::PriceLevelUpdateBuy, "ZIEXT", 25.00, 100);
    order_book.UpdateOrderBook(MessageType::PriceLevelUpdateBuy, "ZIEXT", 24.90, 100);
    order_book.UpdateBBO();
    // Verify initial BBO
    auto initial_bbo = order_book.GetBbo();
    ASSERT_TRUE(initial_bbo.has_value());
    EXPECT_EQ(initial_bbo->getBidPrice(), 25.00);
    EXPECT_EQ(initial_bbo->getBidSize(), 100);
    EXPECT_EQ(initial_bbo->getAskPrice(), 25.10);
    EXPECT_EQ(initial_bbo->getAskSize(), 100);

    // Step 2: Price Level Update with flags 0x0 (in transition)
    auto message_in_transition = std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateSell, "ZIEXT", 25.10, 0, 0x0);
    order_book.ProcessMessage(std::move(message_in_transition));

    // Verify BBO remains in transition state (25.00 x 25.10)
    order_book.UpdateBBO();
    auto bbo_in_transition = order_book.GetBbo();
    ASSERT_TRUE(bbo_in_transition.has_value());
    EXPECT_EQ(bbo_in_transition->getBidPrice(), 25.00);
    EXPECT_EQ(bbo_in_transition->getBidSize(), 100);
    EXPECT_EQ(bbo_in_transition->getAskPrice(), 25.10);
    EXPECT_EQ(bbo_in_transition->getAskSize(), 100);

    // Step 3: Price Level Update with flags 0x1 (transition complete)
    auto message_transition_complete = std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateSell, "ZIEXT", 25.20, 0, 0x1);
    order_book.ProcessMessage(std::move(message_transition_complete));

    // Verify final BBO after transition (25.00 x 25.30)
    auto final_bbo = order_book.GetBbo();
    ASSERT_TRUE(final_bbo.has_value());
    EXPECT_EQ(final_bbo->getBidPrice(), 25.00);
    EXPECT_EQ(final_bbo->getBidSize(), 100);
    EXPECT_EQ(final_bbo->getAskPrice(), 25.30);
    EXPECT_EQ(final_bbo->getAskSize(), 100);
}