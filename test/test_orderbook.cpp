#include "gtest/gtest.h"
#include "orderbook.h"
#include "iex_messages.h"
#include <random>
#include <sstream>

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook order_book;

    void SetUp() override {
        // Optionally initialize the order book
    }

    void TearDown() override {
        // Optionally clean up
    }
};

// Unit test for the OrderBook class
TEST_F(OrderBookTest, BboTransitionTest) {
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
    PriceLevelUpdateMessage message_in_transition(
        MessageType::PriceLevelUpdateSell, "ZIEXT", 25.10, 0, 0x0);
    order_book.ProcessMessage(message_in_transition);

    // Verify BBO remains in transition state (25.00 x 25.10)
    order_book.UpdateBBO();
    auto bbo_in_transition = order_book.GetBbo();
    ASSERT_TRUE(bbo_in_transition.has_value());
    EXPECT_EQ(bbo_in_transition->getBidPrice(), 25.00);
    EXPECT_EQ(bbo_in_transition->getBidSize(), 100);
    EXPECT_EQ(bbo_in_transition->getAskPrice(), 25.10);
    EXPECT_EQ(bbo_in_transition->getAskSize(), 100);

    // Step 3: Price Level Update with flags 0x1 (transition complete)
    PriceLevelUpdateMessage message_transition_complete(
        MessageType::PriceLevelUpdateSell, "ZIEXT", 25.20, 0, 0x1);
    order_book.ProcessMessage(message_transition_complete);

    // Verify final BBO after transition (25.00 x 25.30)
    auto final_bbo = order_book.GetBbo();
    ASSERT_TRUE(final_bbo.has_value());
    EXPECT_EQ(final_bbo->getBidPrice(), 25.00);
    EXPECT_EQ(final_bbo->getBidSize(), 100);
    EXPECT_EQ(final_bbo->getAskPrice(), 25.30);
    EXPECT_EQ(final_bbo->getAskSize(), 100);
}

// Test for calculating book pressure
TEST_F(OrderBookTest, CalculateBookPressure) {
    PriceLevelUpdateMessage buy_message(MessageType::PriceLevelUpdateBuy, "AAPL", 150.0, 100, 1);
    PriceLevelUpdateMessage sell_message(MessageType::PriceLevelUpdateSell, "AAPL", 155.0, 50, 1);

    order_book.ProcessMessage(buy_message);
    order_book.ProcessMessage(sell_message);

    double pressure = order_book.GetBookPressure();
    const double tolerance = 1e-6;

    EXPECT_NEAR(pressure, (100.0 - 50.0) / (100.0 + 50.0), tolerance);
}

// Test for printing BBO when none is available
TEST_F(OrderBookTest, PrintBboWhenNoneAvailable) {
    std::ostringstream oss;
    std::streambuf* original_cout = std::cout.rdbuf(oss.rdbuf());

    order_book.PrintBbo();

    std::cout.rdbuf(original_cout);
    std::string output = oss.str();
    EXPECT_EQ(output, "No Best Bid or Offer available.\n");
}

// Test for handling multiple updates in sequence
TEST_F(OrderBookTest, MultipleUpdatesInSequence) {
    PriceLevelUpdateMessage buy_message1(MessageType::PriceLevelUpdateBuy, "AAPL", 150.0, 100, 1);
    PriceLevelUpdateMessage sell_message1(MessageType::PriceLevelUpdateSell, "AAPL", 155.0, 50, 1);
    PriceLevelUpdateMessage buy_message2(MessageType::PriceLevelUpdateBuy, "AAPL", 148.0, 200, 1);
    PriceLevelUpdateMessage sell_message2(MessageType::PriceLevelUpdateSell, "AAPL", 157.0, 0, 1);

    order_book.ProcessMessage(buy_message1);
    order_book.ProcessMessage(sell_message1);
    order_book.ProcessMessage(buy_message2);
    order_book.ProcessMessage(sell_message2);

    auto bbo = order_book.GetBbo();
    ASSERT_TRUE(bbo.has_value());
    EXPECT_DOUBLE_EQ(bbo->getBidPrice(), 150.0);
    EXPECT_EQ(bbo->getBidSize(), 100);
    EXPECT_DOUBLE_EQ(bbo->getAskPrice(), 155.0);
    EXPECT_EQ(bbo->getAskSize(), 50);
}

// Test for edge case of simultaneous updates on the same price level
TEST_F(OrderBookTest, SimultaneousUpdatesOnSamePriceLevel) {
    PriceLevelUpdateMessage buy_message1(MessageType::PriceLevelUpdateBuy, "AAPL", 150.0, 100, 1);
    PriceLevelUpdateMessage buy_message2(MessageType::PriceLevelUpdateBuy, "AAPL", 150.0, 150, 1);
    PriceLevelUpdateMessage sell_message1(MessageType::PriceLevelUpdateSell, "AAPL", 155.0, 50, 1);
    PriceLevelUpdateMessage sell_message2(MessageType::PriceLevelUpdateSell, "AAPL", 155.0, 60, 1);

    order_book.ProcessMessage(buy_message1);
    order_book.ProcessMessage(buy_message2);
    order_book.ProcessMessage(sell_message1);
    order_book.ProcessMessage(sell_message2);

    auto bbo = order_book.GetBbo();
    ASSERT_TRUE(bbo.has_value());
    EXPECT_DOUBLE_EQ(bbo->getBidPrice(), 150.0);
    EXPECT_EQ(bbo->getBidSize(), 150);
    EXPECT_DOUBLE_EQ(bbo->getAskPrice(), 155.0);
    EXPECT_EQ(bbo->getAskSize(), 60);
}

// Test for handling large number of messages
TEST_F(OrderBookTest, LargeNumberOfMessages) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> bid_price_dist(150.0, 180.0);
    std::uniform_real_distribution<> ask_price_dist(200.0, 250.0);
    std::uniform_int_distribution<> bid_size_dist(100, 200);
    std::uniform_int_distribution<> ask_size_dist(50, 100);

    for (int i = 0; i < 20; ++i) {
        double bid_price = bid_price_dist(gen);
        double ask_price = ask_price_dist(gen);
        int bid_size = bid_size_dist(gen);
        int ask_size = ask_size_dist(gen);

        PriceLevelUpdateMessage buy_message(MessageType::PriceLevelUpdateBuy, "AAPL", bid_price, bid_size, 1);
        PriceLevelUpdateMessage sell_message(MessageType::PriceLevelUpdateSell, "AAPL", ask_price, ask_size, 1);

        order_book.ProcessMessage(buy_message);
        order_book.ProcessMessage(sell_message);
    }

    PriceLevelUpdateMessage final_buy(MessageType::PriceLevelUpdateBuy, "AAPL", 190.0, 50, 1);
    PriceLevelUpdateMessage final_sell(MessageType::PriceLevelUpdateSell, "AAPL", 191.0, 50, 1);

    order_book.ProcessMessage(final_buy);
    order_book.ProcessMessage(final_sell);

    auto bbo = order_book.GetBbo();
    ASSERT_TRUE(bbo.has_value());

    EXPECT_DOUBLE_EQ(bbo->getBidPrice(), 190.0);
    EXPECT_EQ(bbo->getBidSize(), 50);
    EXPECT_DOUBLE_EQ(bbo->getAskPrice(), 191.0);
    EXPECT_EQ(bbo->getAskSize(), 50);
    EXPECT_LT(bbo->getBidPrice(), bbo->getAskPrice());
}
