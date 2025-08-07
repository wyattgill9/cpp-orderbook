#pragma once

#include <deque>
#include <map>
#include <unordered_map>
#include <variant>
#include <atomic>
#include <thread>
#include <iostream>
#include <cstring>
#include <string>

#include "SPSCQueue.h"
#include "util.hpp"

struct Order {
    u64 order_reference_id;

    std::byte side;
    OrderExecutionType execution_type;
    TimeInForce time_in_force;

    f32 price;
    u32 quantity;
    u64 timestamp_ns;
    bool has_price;

    friend std::ostream& operator<<(std::ostream& os, const Order& ord);
};

using OrderMessage = std::variant<
    std::monostate,
    Order,
    AddOrderNoMPIDMessage,
    OrderDeleteMessage,
    OrderCancelMessage,
    OrderExecutedMessage
>;

class OrderBook {
public:
    OrderBook();
    OrderBook(const std::string& sym, f32 ts = 0.01);

    ~OrderBook();

    void add_order_to_book(const Order& order);
    void add_order(f32 price, u32 quantity, char side);
    void submit_message(const OrderMessage& message);
    void edit_book(const std::byte* ptr, size_t size);

    f32 get_best_bid();
    f32 get_best_ask();
    f32 get_tick_size() const;

    void print() const;
    const std::string get_symbol() const;

private:
    std::unordered_map<u64, Order> order_id_map;
    std::map<f32, std::deque<u64>, std::greater<f32>> bids;
    std::map<f32, std::deque<u64>> asks;
    
    u64 last_order_id; // Only used when add order is called without id param   
    std::string symbol;
    f32 tick_size;

    rigtorp::SPSCQueue<OrderMessage> message_queue;
    std::atomic<bool> running {false};
    std::thread processing_thread;

    void start();
    void stop();

    void process_messages();
    void process_message(const OrderMessage& msg);

    // void add_order_to_book(const Order& order);
    Order& get_order_from_id(u64 order_id);
    Order remove_order_from_id(u64 order_id);
    void cancel_order(u64 order_id, u32 cancelled_shares);
    void execute_order(u64 order_id, u32 executed_shares, u64 match_order_id);

    void replace_order(u64 original_order_id, u64 new_order_id, u32 shares, f32 price);
};

// NEW
// static constexpr size_t MAX_LEVELS = 1000;

// PriceLevel price_levels[MAX_LEVELS];
// std::unordered_map<f32, size_t> price_to_index;

// std::unordered_map<u64, Order> order_id_map;
