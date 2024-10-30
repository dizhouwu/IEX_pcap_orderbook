#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "iex_decoder.h"
#include "iex_messages.h"
#include "orderbook.h"

std::string parseBusinessDate(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of('/');
    if (lastSlash == std::string::npos) return "";
    std::string fileName = filePath.substr(lastSlash + 1);
    size_t underscorePos = fileName.find('_');
    if (underscorePos == std::string::npos) return "";
    return fileName.substr(0, underscorePos);
}

uint64_t businessDateToNanosecondsSinceEpoch(const std::string& businessDate) {
    if (businessDate.size() != 8) throw std::invalid_argument("Invalid date format. Expected format: YYYYMMDD");

    std::tm tm = {};
    std::istringstream ss(businessDate);
    ss >> std::get_time(&tm, "%Y%m%d");
    if (ss.fail()) throw std::runtime_error("Failed to parse date.");

    tm.tm_hour = 9;
    tm.tm_min = 30;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;

    std::time_t time = std::mktime(&tm);
    if (time == -1) throw std::runtime_error("Failed to convert to time_t.");

    auto epoch = std::chrono::system_clock::from_time_t(time);
    return std::chrono::duration_cast<std::chrono::nanoseconds>(epoch.time_since_epoch()).count();
}

std::string nanosSinceEpochToTimestamp(int64_t nanos_since_epoch) {
    auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>(std::chrono::nanoseconds(nanos_since_epoch));
    std::time_t seconds = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&seconds), "%F %T");

    auto duration_since_epoch = tp.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch) % 1000;
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration_since_epoch) % 1000;
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epoch) % 1000;

    oss << "." << std::setw(3) << std::setfill('0') << milliseconds.count()
        << std::setw(3) << microseconds.count()
        << std::setw(3) << nanoseconds.count();

    return oss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: iex_pcap_decoder <input_pcap>" << std::endl;
        return 1;
    }

    std::ofstream out_stream;
    try {
        out_stream.open("quotes.csv");
    } catch (...) {
        std::cout << "Exception thrown opening output file." << std::endl;
        return 1;
    }

    out_stream << "Timestamp,Symbol,BidSize,BidPrice,AskSize,AskPrice" << std::endl;
    std::string input_file(argv[1]);
    auto biz_date = parseBusinessDate(input_file);
    auto biz_nano_open = businessDateToNanosecondsSinceEpoch(biz_date);
    IEXDecoder decoder;

    if (!decoder.OpenFileForDecoding(input_file)) {
        std::cout << "Failed to open file '" << input_file << "'." << std::endl;
        return 1;
    }

    std::vector<std::unique_ptr<IEXMessageBase>> messages;
    std::unique_ptr<IEXMessageBase> msg_ptr;
    std::cout << "Starting decoding pcaps.." << std::endl;
    auto ret_code = decoder.GetNextMessage(msg_ptr);
    while (ret_code == ReturnCode::Success) {
        messages.push_back(std::move(msg_ptr));
        ret_code = decoder.GetNextMessage(msg_ptr);
    }
    std::cout << "Decoding pcap is done.." << std::endl;
    std::sort(messages.begin(), messages.end(), [](const std::unique_ptr<IEXMessageBase>& a, const std::unique_ptr<IEXMessageBase>& b) {
        return a->timestamp < b->timestamp;
    });

    std::unordered_map<std::string, OrderBook> order_books;

    for (const auto& message : messages) {
        auto* msg_base = message.get();
        std::string symbol = msg_base->GetSymbol();

        if (symbol == "TSLA" || msg_base->GetMessageType() == MessageType::PriceLevelUpdateBuy || msg_base->GetMessageType() == MessageType::PriceLevelUpdateSell) {
            auto ts = nanosSinceEpochToTimestamp(msg_base->timestamp);
            out_stream << ts << "," << MessageTypeToString(msg_base->GetMessageType()) << std::endl;
        }

        if (msg_base->timestamp >= biz_nano_open) {
            if (!symbol.empty() && symbol == "TSLA") {
                auto& ob = order_books[symbol];
                ob.ProcessMessage(*message);

                double book_pressure = ob.GetBookPressure();
                std::cout << "Symbol: " << symbol << ", Book Pressure: " << book_pressure << std::endl;
                ob.PrintOrderBook();
                ob.PrintBbo();
            }
        }
    }

    out_stream.close();
    return 0;
}
