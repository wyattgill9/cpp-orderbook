#pragma once

#include <deque>
#include <map>
#include <unordered_map>
#include <variant>
#include <atomic>
#include <thread>
#include <iostream>
#include <cstring>
#include <list>

#include "SPSCQueue.h"
#include "util.hpp"
#include "messages.h"

using OrderMessage = std::variant<
    std::monostate,
    AddOrderNoMPIDMessage,
    OrderDeleteMessage,
    OrderCancelMessage,
    OrderExecutedMessage
>;

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

struct Order {
    u64 order_reference_id;

    OrderSide side;
    OrderExecutionType execution_type;
    TimeInForce time_in_force;

    f32 price;
    u32 quantity;
    u64 timestamp_ns;
    bool has_price;

    friend std::ostream& operator<<(std::ostream& os, const Order& ord);
};

class PriceLevel {
private:
    f32 price;
    std::list<Order> orders; 
};

class OrderBook {
public:
    OrderBook();
    OrderBook(const std::string& sym, f32 ts = 0.01);

    void submit_message(const OrderMessage& message);
    void start();
    void stop();
    void edit_book(const std::byte* ptr, size_t size);
    void print() const;

    f32 get_best_bid();
    f32 get_best_ask();
    const char* get_symbol() const;
    f32 get_tick_size() const;

private:
    std::unordered_map<u64, Order> orders;
    std::map<f32, std::deque<u64>, std::greater<f32>> bids;
    std::map<f32, std::deque<u64>> asks;

    char symbol[8] {0};
    f32 tick_size;

    rigtorp::SPSCQueue<OrderMessage> message_queue;
    std::atomic<bool> running {false};
    std::thread processing_thread;

    void process_messages();
    void process_message(const OrderMessage& msg);
    void add_order(const Order& order);
    Order& get_order_from_id(u64 order_id);
    Order remove_order_from_id(u64 order_id);
    void cancel_order(u64 order_id, u32 cancelled_shares);
    void execute_order(u64 order_id, u32 executed_shares, u64 match_order_id);
};
