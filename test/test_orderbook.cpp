

#include "gtest/gtest.h"
#include "orderbook.h"
#include "iex_messages.h"
#include <random>
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






// 6. Test for calculating book pressure
TEST_F(OrderBookTest, CalculateBookPressure) {
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateBuy, "AAPL", 150.0, 100, 1));
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateSell, "AAPL", 155.0, 50, 1));

    double pressure = order_book.GetBookPressure();
    const double tolerance = 1e-6; // You can adjust this as needed

    EXPECT_NEAR(pressure, (100.0 - 50.0) / (100.0 + 50.0), tolerance);
}

// 7. Test for printing BBO when none is available
TEST_F(OrderBookTest, PrintBboWhenNoneAvailable) {
    std::ostringstream oss;
    std::streambuf* original_cout = std::cout.rdbuf(oss.rdbuf());

    order_book.PrintBbo();

    std::cout.rdbuf(original_cout);
    std::string output = oss.str();
    EXPECT_EQ(output, "No Best Bid or Offer available.\n");
}

// 8. Test for handling multiple updates in sequence
TEST_F(OrderBookTest, MultipleUpdatesInSequence) {
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateBuy, "AAPL", 150.0, 100, 1));
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateSell, "AAPL", 155.0, 50, 1));
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateBuy, "AAPL", 148.0, 200, 1));
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateSell, "AAPL", 157.0, 0, 1)); // Remove the price level

    auto bbo = order_book.GetBbo();
    ASSERT_TRUE(bbo.has_value());
    EXPECT_DOUBLE_EQ(bbo->getBidPrice(), 150.0);
    EXPECT_EQ(bbo->getBidSize(), 100);
    EXPECT_DOUBLE_EQ(bbo->getAskPrice(), 155.0);
    EXPECT_EQ(bbo->getAskSize(), 50);
}



// 13. Test for edge case of simultaneous updates on the same price level
TEST_F(OrderBookTest, SimultaneousUpdatesOnSamePriceLevel) {
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateBuy, "AAPL", 150.0, 100, 1));
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateBuy, "AAPL", 150.0, 150, 1));
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateSell, "AAPL", 155.0, 50, 1));
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateSell, "AAPL", 155.0, 60, 1));

    auto bbo = order_book.GetBbo();
    ASSERT_TRUE(bbo.has_value());
    EXPECT_DOUBLE_EQ(bbo->getBidPrice(), 150.0);
    EXPECT_EQ(bbo->getBidSize(), 150); // Expect updated size
    EXPECT_DOUBLE_EQ(bbo->getAskPrice(), 155.0);
    EXPECT_EQ(bbo->getAskSize(), 60); // Expect updated size
}


// 16. Test for handling large number of messages
TEST_F(OrderBookTest, LargeNumberOfMessages) {
    std::random_device rd; // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator
    std::uniform_real_distribution<> bid_price_dist(150.0, 180.0); // Range for bid prices
    std::uniform_real_distribution<> ask_price_dist(200.0, 250.0); // Range for ask prices
    std::uniform_int_distribution<> bid_size_dist(100, 200); // Range for bid sizes
    std::uniform_int_distribution<> ask_size_dist(50, 100); // Range for ask sizes

    for (int i = 0; i < 20; ++i) {
        // Generate random bid and ask prices and sizes
        double bid_price = bid_price_dist(gen);
        double ask_price = ask_price_dist(gen);
        int bid_size = bid_size_dist(gen);
        int ask_size = ask_size_dist(gen);

        // Process a random bid update message
        order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
            MessageType::PriceLevelUpdateBuy, "AAPL", bid_price, bid_size, 1));

        // Process a random ask update message
        order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
            MessageType::PriceLevelUpdateSell, "AAPL", ask_price, ask_size, 1));
    }
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateBuy, "AAPL", 190.0, 50, 1));

    // Process a random ask update message
    order_book.ProcessMessage(std::make_unique<PriceLevelUpdateMessage>(
        MessageType::PriceLevelUpdateSell, "AAPL", 191.0, 50, 1));
    // Retrieve the best bid and ask from the order book
    auto bbo = order_book.GetBbo();
    ASSERT_TRUE(bbo.has_value());

    // Verify the bid and ask prices and sizes (use assertions based on your expectations)
    EXPECT_DOUBLE_EQ(bbo->getBidPrice(), 190.0); // Replace with expected value if necessary
    EXPECT_EQ(bbo->getBidSize(), 50); // Replace with expected value if necessary
    EXPECT_DOUBLE_EQ(bbo->getAskPrice(), 191.0); // Replace with expected value if necessary
    EXPECT_EQ(bbo->getAskSize(), 50); // Replace with expected value if necessary

    // Ensure that the bid is always less than the ask
    EXPECT_LT(bbo->getBidPrice(), bbo->getAskPrice()); // Bid price must be less than ask price
}