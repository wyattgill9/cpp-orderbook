#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <array>
#include <iterator>
#include <print>
#include <iostream>
#include <cctype>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <map>
#include <deque>
#include <variant>
#include <thread>
#include <stop_token>

// #include "FixedRingBuffer.h"
#include "SPSCQueue.h"

template<class ...Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

#include "messages.h"

#define DEBUG 0

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

    friend std::ostream& operator<<(std::ostream& os, const Order& ord) {
            os << "Order(id=" << ord.order_reference_id
                << ", side=" << to_underlying(ord.side)
                << ", policy=" << to_underlying(ord.execution_type)
                << ", time_in_force=" << to_underlying(ord.time_in_force)
                << ", price=";
            if(ord.has_price)
                os << ord.price;
            else
                os << "market";

            os << ", quantity=" << ord.quantity
                << ", timestamp=" << ord.timestamp_ns << "ns" 
                << ")";
            return os;
    }   
};

class Logger {
    
public:
    Logger() = default;

};

using OrderMessage = std::variant<
    std::monostate,
    AddOrderNoMPIDMessage,
    OrderDeleteMessage,
    OrderCancelMessage,
    OrderExecutedMessage
>;

class OrderBook {  
    size_t observer_count {0};

    // Store orders by ID
    std::unordered_map<u64, Order> orders;
        
    // Price levels just store order IDs, not copies
    std::map<f32, std::deque<u64>, std::greater<f32>> bids;
    std::map<f32, std::deque<u64>> asks;    
    // BoundedQueue<Order, 1000000> orders;
    
    char symbol[8] {0};
    f32 tick_size;

    // SPSC Queue for incoming messages
    rigtorp::SPSCQueue<OrderMessage> message_queue;
    std::atomic<bool> running {false};
    std::thread processing_thread;


    void process_messages() {
        while(running) {
            OrderMessage* msg = message_queue.front();
        
            if(!std::holds_alternative<std::monostate>(*msg)) {
                process_message(*msg);
            }

            message_queue.pop();
        }

        while(true) {
            OrderMessage* msg = message_queue.front();
            if(msg == nullptr) break;
        
            process_message(*msg);
            message_queue.pop();
        }
    }
    
    void process_message(const OrderMessage& msg) {
        std::visit(overloaded {
            [this] (const AddOrderNoMPIDMessage& msg) {
                Order order {
                    .order_reference_id = msg.order_reference_number,
                    .side = char_to_order_side(msg.buy_sell_indicator),
                    .execution_type = OrderExecutionType::LIMIT,
                    .time_in_force = TimeInForce::GTC,
                    .price = msg.price,
                    .quantity = msg.shares,
                    .timestamp_ns = msg.header.timestamp,
                    .has_price = true
                };            

                add_order(order);
            },
            [this] (const OrderDeleteMessage& msg) {    
                remove_order_from_id(msg.order_reference_number);
            },
            [this] (const OrderCancelMessage& msg) {
                cancel_order(msg.order_reference_number, msg.cancelled_shares);
            },
            [this] (const OrderExecutedMessage& msg) {
                execute_order(msg.order_reference_number, msg.executed_shares, msg.match_number);
            },
            [this] (const std::monostate) {}
        }, msg);

        print();
    }

    void add_order(const Order& order) {
        if (!order.has_price) {
            return;
        }

        orders[order.order_reference_id] = order;
        
        if (order.side == OrderSide::BUY) {
            bids[order.price].push_back(order.order_reference_id);
        } else {
            asks[order.price].push_back(order.order_reference_id);
        }
    }

    Order& get_order_from_id(u64 order_id) {
        return orders.at(order_id);
    }

    Order remove_order_from_id(u64 order_id) {
        Order order = orders.at(order_id);
        
        auto& level = (order.side == OrderSide::BUY) ? bids[order.price] : asks[order.price];
        level.erase(std::find(level.begin(), level.end(), order_id));
        
        if (level.empty()) {
            if (order.side == OrderSide::BUY) {
                bids.erase(order.price);
            } else {
                asks.erase(order.price);
            }
        }
        
        orders.erase(order_id);
        
        return order;
    }

    void cancel_order(u64 order_id, u32 cancelled_shares) {
        Order& order = get_order_from_id(order_id);
        order.quantity -= cancelled_shares;

        if (order.quantity == 0) {
            remove_order_from_id(order_id);
        }       
    }   

    void execute_order(u64 order_id, u32 executed_shares, u64 match_order_id) {
        Order& order = get_order_from_id(order_id);
        order.quantity -= executed_shares;

        Order& match_order = get_order_from_id(match_order_id);
        match_order.quantity -= executed_shares;
       
        if (order.quantity == 0) {
            remove_order_from_id(order_id);
        }

        if(match_order.quantity == 0) {
            remove_order_from_id(match_order_id);
        }
    }

    constexpr size_t get_message_size(std::byte c) {
        switch (static_cast<char>(c)) {
            case 'A': return sizeof(AddOrderNoMPIDMessage);
            case 'D': return sizeof(OrderDeleteMessage);
            case 'X': return sizeof(OrderCancelMessage);
            case 'E': return sizeof(OrderExecutedMessage);
            default: return 0;
        }
    }

    OrderSide char_to_order_side(std::byte indicator) {
        return (static_cast<char>(indicator) == 'B') ? OrderSide::BUY : OrderSide::SELL;
    }

public:    
    OrderBook() : message_queue(10000) {} 

    OrderBook(const std::string& sym, f32 ts = 0.01)
        : tick_size(ts), message_queue(10000) {
        std::snprintf(symbol, sizeof(symbol), "%s", sym.c_str());
    }

    void submit_message(const OrderMessage& message) {
        std::ignore = message_queue.try_push(message);
    }

    void start() {
        running = true;
        processing_thread = std::thread(&OrderBook::process_messages, this);
    }

    void stop() {
        running = false;
        if(processing_thread.joinable()) {
            processing_thread.join();
        } 
    }

    void edit_book(const std::byte* ptr, size_t size) {
        const std::byte* end = ptr + size;

        while (ptr < end) {
            std::byte type = *ptr;
            size_t msg_size = get_message_size(type);
            if (msg_size == 0 || ptr + msg_size > end) break;

            switch(static_cast<char>(type)) {
                case 'A' : {
                    const AddOrderNoMPIDMessage* msg = reinterpret_cast<const AddOrderNoMPIDMessage*>(ptr);

                    Order order {
                        .order_reference_id = msg->order_reference_number,
                        .side = char_to_order_side(msg->buy_sell_indicator),
                        .execution_type = OrderExecutionType::LIMIT,
                        .time_in_force = TimeInForce::GTC,
                        .price = msg->price,
                        .quantity = msg->shares,
                        .timestamp_ns = msg->header.timestamp,
                        .has_price = true 
                    };

                    add_order(order);

                    break;
                }
                case 'D' : {
                    const OrderDeleteMessage* msg = reinterpret_cast<const OrderDeleteMessage*>(ptr);    
                    remove_order_from_id(msg->order_reference_number);
                    break;
                }
                case 'X' : {
                    const OrderCancelMessage* msg = reinterpret_cast<const OrderCancelMessage*>(ptr);    
                    cancel_order(msg->order_reference_number, msg->cancelled_shares);
                    break;
                }
                case 'E' : {
                    const OrderExecutedMessage* msg = reinterpret_cast<const OrderExecutedMessage*>(ptr);    
                    execute_order(msg->order_reference_number, msg->executed_shares, msg->match_number);
                    break;
                }
            }

            #if DEBUG
                print();
            #endif

            ptr += msg_size;
        }
    }

    void print() const {
        std::cout << "--- BIDS ---" << std::endl;
        for (const auto& [price, order_ids] : bids) {
            std::cout << "Price " << price << ":" << std::endl;
            for (u64 order_id : order_ids) {
                std::cout << "  " << orders.at(order_id) << std::endl;
            }
        }

        std::cout << "--- ASKS ---" << std::endl;
        for (const auto& [price, order_ids] : asks) {
            std::cout << "Price " << price << ":" << std::endl;
            for (u64 order_id : order_ids) {
                std::cout << "  " << orders.at(order_id) << std::endl;
            }
        }
        std::cout << std::endl;
    }
 
    f32 get_best_bid() { return bids.begin()->first; }
    f32 get_best_ask() { return asks.begin()->first; }
    const char* get_symbol() const { return symbol; }
    f32 get_tick_size() const { return tick_size; }
    size_t get_observer_count() const { return observer_count; }
};

int main() {
    auto ob = OrderBook("TSLA");

    Logger logger;
   
    ob.start();
    // std::byte buffer[1024];

    AddOrderNoMPIDMessage add_order = {
        .header = {
            .message_type = 'A',
            .stock_locate = 0,
            .tracking_number = 0,
            .timestamp = 1
        },
        .order_reference_number = 1,
        .buy_sell_indicator = static_cast<std::byte>('B'),
        .shares = 1000,
        .stock = "AAPL",
        .price = 0.01
    };

    // OrderDeleteMessage cancel_order = {
    //     .header = {
    //         .message_type = 'D',
    //         .stock_locate = 0,
    //         .tracking_number = 0,
    //         .timestamp = 1
    //     },
    //     .order_reference_number = 1,
    // };
    // std::memcpy(buffer, &add_order, sizeof(add_order));
    // std::memcpy(buffer + sizeof(add_order), &cancel_order, sizeof(cancel_order));

    ob.submit_message(add_order);

    ob.stop();

    return 0;
}
