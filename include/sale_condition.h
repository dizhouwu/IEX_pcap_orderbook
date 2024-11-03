#pragma once
#include <cstdint>

enum SaleCondition : uint8_t {
    // Individual flag bits
    IntermarketSweepFlag = 0x80,  
    ExtendedHoursFlag    = 0x40,  
    OddLotFlag           = 0x20,  
    TradeThroughExemptFlag = 0x10, 
    SinglePriceCrossTradeFlag = 0x08 
};

static inline bool isIntermarketSweep(uint8_t flags) {
    return (flags & IntermarketSweepFlag)!= 0;
}

static inline bool isExtendedHours(uint8_t flags) {
    return (flags & ExtendedHoursFlag)!= 0;
}

static inline bool isOddLot(uint8_t flags) {
    return (flags & OddLotFlag)!= 0;
}

static inline bool isTradeThroughExempt(uint8_t flags) {
    return (flags & TradeThroughExemptFlag)!= 0;
}

static inline bool isSinglePriceCrossTrade(uint8_t flags) {
    return (flags & SinglePriceCrossTradeFlag)!= 0;
}