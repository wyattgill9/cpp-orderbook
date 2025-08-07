#pragma once

#include <chrono>

template<class ...Ts> struct overloaded : Ts... { using Ts::operator()...; };

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

enum class OrderSide {
    BUY = 0,
    SELL = 1
};

enum class OrderExecutionType {
    MARKET = 0,
    LIMIT = 1,
};

enum class TimeInForce {
    DAY = 0,
    GTC = 1,
    IOC = 2,
    FOK = 3
 };

inline OrderSide byte_to_order_side(std::byte c) {
    return (static_cast<char>(c) == 'B') ? OrderSide::BUY : OrderSide::SELL;
}

inline OrderSide byte_to_order_side(char c) {
    return (c == 'B') ? OrderSide::BUY : OrderSide::SELL;
}

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
    std::byte buy_sell_indicator; // use to be u8
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

// 'U'
struct __attribute__((packed)) OrderReplaceMessage {
    MessageHeader header;
    u64 original_order_reference_number;
    u64 new_order_reference_number;
    u32 shares;
    f32 price;
};


constexpr size_t get_message_size(char c) {
    switch (c) {
        case 'A': return sizeof(AddOrderNoMPIDMessage);
        case 'D': return sizeof(OrderDeleteMessage);
        case 'X': return sizeof(OrderCancelMessage);
        case 'E': return sizeof(OrderExecutedMessage);
        default: return 0;
    }
}

inline u64 get_ns_from_midnight() {
    using namespace std::chrono;
    
    auto now = system_clock::now();

    // 1970-01-01 00:00:00 UTC - midnight
    auto time_since_epoch = now.time_since_epoch();
    auto ns_since_epoch = duration_cast<nanoseconds>(time_since_epoch).count();
    
    constexpr uint64_t NS_PER_DAY = 24ULL * 60ULL * 60ULL * 1000000000ULL;
    
    return ns_since_epoch % NS_PER_DAY;
}

