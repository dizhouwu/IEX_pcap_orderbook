#pragma once
enum class Side { Buy = 0x38, Sell = 0x35};

enum class ModifyFlags { ResetPriority = 0, MaintainPriority = 1};

struct Order {
    uint8_t order_id;
    uint8_t size;
    double price;
    Side side; // Add the side property to track whether it's a buy or sell order
};