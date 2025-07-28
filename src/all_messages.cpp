#pragma once
#include "util.cpp"

struct __attribute__((packed)) MessageHeader {
    u16 stock_locate;
    u16 tracking_number;
    u64 timestamp : 48;  // 48-bit timestamp
};

// 'S'
struct __attribute__((packed)) SystemEventMessage {
    MessageHeader header;
    u8 event_code;
};

// 'R'
struct __attribute__((packed)) StockDirectoryMessage {
    MessageHeader header;
    char stock[8];
    u8 market_category;
    u8 financial_status_indicator;
    u32 round_lot_size;
    u8 round_lots_only;
    u8 issue_classification;
    char issue_sub_type[2];
    u8 authenticity;
    u8 short_sale_threshold_indicator;
    u8 ipo_flag;
    u8 luld_reference_price_tier;
    u8 etp_flag;
    u32 etp_leverage_factor;
    u8 inverse_indicator;
};

// 'H'
struct __attribute__((packed)) StockTradingActionMessage {
    MessageHeader header;
    char stock[8];
    u8 trading_state;
    u8 reserved;
    char reason[4];
};

// 'L'
struct __attribute__((packed)) MarketParticipantPositionMessage {
    MessageHeader header;
    char market_participant_id[4];
    char stock[8];
    u8 primary_market_maker;
    u8 market_maker_mode;
    u8 market_participant_state;
};

// 'Y'
struct __attribute__((packed)) ShortSalePriceTestMessage {
    MessageHeader header;
    char stock[8];
    u8 reg_sho_action;
};

// 'V'
struct __attribute__((packed)) MWCBDeclineLevelMessage {
    MessageHeader header;
    f32 level_one_price;
    f32 level_two_price;
    f32 level_three_price;
};

// 'W'
struct __attribute__((packed)) MWCBStatusMessage {
    MessageHeader header;
    u8 breached_level;
};

// 'K'
struct __attribute__((packed)) QuotingPeriodUpdateMessage {
    MessageHeader header;
    u32 ipo_quotation_release_time;
    u8 ipo_quotation_release_qualifier;
    f32 ipo_price;
};

// 'J'
struct __attribute__((packed)) LULDAuctionCollarMessage {
    MessageHeader header;
    char stock[8];
    f32 auction_caller_reference_price;
    f32 upper_auction_collar_price;
    f32 lower_auction_collar_price;
    u32 auction_caller_extension;
};

// 'h'
struct __attribute__((packed)) OperationalHaltMessage {
    MessageHeader header;
    char stock[8];
    u8 market_code;
    u8 operation_halt_message;
};

// 'A'
struct __attribute__((packed)) AddOrderNoMPIDMessage {
    MessageHeader header;
    u64 order_reference_number;
    u8 buy_sell_indicator;
    u32 shares;
    char stock[8];
    f32 price;
};

// 'F'
struct __attribute__((packed)) AddOrderWithMPIDMessage {
    MessageHeader header;
    u64 order_reference_number;
    u8 buy_sell_indicator;
    u32 shares;
    char stock[8];
    f32 price;
    char attribution[4];
};

// 'E'
struct __attribute__((packed)) OrderExecutedMessage {
    MessageHeader header;
    u64 order_reference_number;
    u32 executed_shares;
    u64 match_number;
};

// 'C'
struct __attribute__((packed)) OrderExecutedwithPriceMessage {
    MessageHeader header;
    u64 order_reference_number;
    u32 executed_shares;
    u64 match_number;
    u8 printable;
    f32 execution_price;
};

// 'X'
struct __attribute__((packed)) OrderCancelMessage {
    MessageHeader header;
    u64 order_reference_number;
    u32 cancelled_shares;
};

// 'D'
struct __attribute__((packed)) OrderDeleteMessage {
    MessageHeader header;
    u64 order_reference_number;
};

// 'U'
struct __attribute__((packed)) OrderReplaceMessage {
    MessageHeader header;
    u64 original_order_reference_number;
    u64 new_order_reference_number;
    u32 shares;
    f32 price;
};

// 'P'
struct __attribute__((packed)) TradeMessage {
    MessageHeader header;
    u64 order_reference_number;
    u8 buy_sell_indicator;
    u32 shares;
    char stock[8];
    f32 price;
    u64 match_number;
};

// 'Q'
struct __attribute__((packed)) CrossTradeMessage {
    MessageHeader header;
    u64 shares;
    char stock[8];
    f32 cross_price;
    u64 match_number;
    u8 cross_type;
};

// 'B'
struct __attribute__((packed)) BrokenTradeMessage {
    MessageHeader header;
    u64 match_number;
};

// 'I'
struct __attribute__((packed)) NOIIMessage {
    MessageHeader header;
    u64 paired_shares;
    u64 imbalance_shares;
    u8 imbalance_direction;
    char stock[8];
    f32 far_price;
    f32 near_price;
    f32 current_reference_price;
    u8 cross_type;
    u8 price_variation_indicator;
};

// 'N'
struct __attribute__((packed)) DirectListingWithCapitalRaisePriceMessage {
    MessageHeader header;
    char stock[8];
    u8 open_eligibility_status;
    f32 minimum_allowable_price;
    f32 maximum_allowable_price;
    f32 near_execution_price;
    u64 near_execution_time;
    f32 lower_price_range_collar;
    f32 upper_price_range_collar;
};
