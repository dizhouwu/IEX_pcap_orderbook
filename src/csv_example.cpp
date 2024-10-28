#include "iex_decoder.h"
#include "orderbook.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>


std::string parseBusinessDate(const std::string& filePath) {
    // Find the last '/' to get the filename
    size_t lastSlash = filePath.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return ""; // Invalid path
    }

    // Extract the filename from the file path
    std::string fileName = filePath.substr(lastSlash + 1);
    
    // Find the position of the first underscore
    size_t underscorePos = fileName.find('_');
    if (underscorePos == std::string::npos) {
        return ""; // No underscore found
    }

    // Extract the business date (the part before the first underscore)
    std::string businessDate = fileName.substr(0, underscorePos);
    return businessDate;
}

uint64_t businessDateToNanosecondsSinceEpoch(const std::string& businessDate) {
    if (businessDate.size() != 8) {
        throw std::invalid_argument("Invalid date format. Expected format: YYYYMMDD");
    }

    // Parse the date string
    std::tm tm = {};
    std::istringstream ss(businessDate);
    ss >> std::get_time(&tm, "%Y%m%d");
    if (ss.fail()) {
        throw std::runtime_error("Failed to parse date.");
    }

    // Set the time to 9:30 AM Eastern Time
    tm.tm_hour = 9;
    tm.tm_min = 30;
    tm.tm_sec = 0;
    tm.tm_isdst = -1; // Let mktime determine whether it's DST

    // Convert to time_t
    std::time_t time = std::mktime(&tm);
    if (time == -1) {
        throw std::runtime_error("Failed to convert to time_t.");
    }

    // Convert to nanoseconds since epoch
    auto epoch = std::chrono::system_clock::from_time_t(time);
    auto duration = epoch.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

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
  auto biz_date = parseBusinessDate(input_file);
  auto biz_nano_open = businessDateToNanosecondsSinceEpoch(biz_date);
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

      if (msg_base->timestamp >= biz_nano_open){
      if ((!symbol.empty()) && (symbol == "TSLA")){
          // Ensure there's an OrderBook for the symbol; create if it doesn't exist.
          auto& ob = order_books[symbol];

          // Process the message for the symbol-specific order book.
          ob.ProcessMessage(std::move(msg_ptr));

          // Print book pressure after each message.
          double book_pressure = ob.GetBookPressure();
          std::cout << "Symbol: " << symbol << ", Book Pressure: " << book_pressure << std::endl;
          ob.PrintOrderBook();
          ob.PrintBbo();
          
      }

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
