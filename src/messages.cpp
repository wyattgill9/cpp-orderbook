#pragma once
#include "util.cpp"

struct __attribute__((packed)) MessageHeader {
    u8 message_type;
    u16 stock_locate;
    u16 tracking_number;
    u64 timestamp : 48;  // 48-bit timestamp
};

// -------- MESSAGES --------
// 'A'
struct __attribute__((packed)) AddOrderNoMPIDMessage {
    MessageHeader header;
    u64 order_reference_number;
    u8 buy_sell_indicator;
    u32 shares;
    char stock[8];
    f32 price;
};

// 'D'
struct __attribute__((packed)) OrderDeleteMessage {
    MessageHeader header;
    u64 order_reference_number;
};

// 'X'
struct __attribute__((packed)) OrderCancelMessage {
    MessageHeader header;
    u64 order_reference_number;
    u32 cancelled_shares;
};

// modify order, reduce amount
// 'E'
struct __attribute__((packed)) OrderExecutedMessage {
    MessageHeader header;
    u64 order_reference_number;
    u32 executed_shares;
    u64 match_number;
};
