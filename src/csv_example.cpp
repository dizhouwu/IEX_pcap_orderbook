#include "iex_decoder.h"
#include "orderbook.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
  // Get the input pcap file as an argument.
  if (argc < 2) {
    std::cout << "Usage: iex_pcap_decoder <input_pcap>" << std::endl;
    return 1;
  }

  // Open a file stream for writing output to csv.
  std::ofstream out_stream;
  try {
    out_stream.open("quotes.csv");
  } catch (...) {
    std::cout << "Exception thrown opening output file." << std::endl;
    return 1;
  }

  // Add the header.
  out_stream << "Timestamp,Symbol,BidSize,BidPrice,AskSize,AskPrice" << std::endl;

  // Initialize decoder object with file path.
  std::string input_file(argv[1]);
  IEXDecoder decoder;
  if (!decoder.OpenFileForDecoding(input_file)) {
    std::cout << "Failed to open file '" << input_file << "'." << std::endl;
    return 1;
  }
  std::unordered_map<std::string, OrderBook> order_books;

  // Get the first message from the pcap file.
  std::unique_ptr<IEXMessageBase> msg_ptr;
  auto ret_code = decoder.GetNextMessage(msg_ptr);
  // Main loop to process all messages.
  for (; ret_code == ReturnCode::Success; ret_code = decoder.GetNextMessage(msg_ptr)) {
      auto* msg_base = msg_ptr.get();
      std::string symbol = msg_base->GetSymbol();

      if ((!symbol.empty()) && (symbol == "ZIEXT")){
          // Ensure there's an OrderBook for the symbol; create if it doesn't exist.
          auto& ob = order_books[symbol];

          // Process the message for the symbol-specific order book.
          ob.ProcessMessage(std::move(msg_ptr));

          // Print book pressure after each message.
          double book_pressure = ob.GetBookPressure();
          std::cout << "Symbol: " << symbol << ", Book Pressure: " << book_pressure << std::endl;
          ob.PrintBbo();
          ob.PrintOrderBook();
      }


  
      // // Optional: Check if it's a PriceLevelUpdate for 'AMD' to write details to the output file.
      // if (msg_base->GetMessageType() == MessageType::PriceLevelUpdateBuy || 
      //     msg_base->GetMessageType() == MessageType::PriceLevelUpdateSell) {

      //     auto* quote_msg = dynamic_cast<PriceLevelUpdateMessage*>(msg_base);

      //     if (quote_msg && quote_msg->symbol == "AMD") {
      //         out_stream << quote_msg->timestamp << ","
      //                     << quote_msg->symbol << ","
      //                     << quote_msg->size << ","
      //                     << quote_msg->price << std::endl;
      //     }
      // }
      // Move to the next message in the stream
      ret_code = decoder.GetNextMessage(msg_ptr);
  }
  out_stream.close();
  return 0;
}
