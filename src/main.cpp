#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <array>
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

#include "SPSCQueue.h"

template<class ...Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// #include "FixedRingBuffer.h"
#include "messages.cpp"

#define DEBUG 1

class IOrderBookObserver {
public:
    virtual void onOrderBookUpdate() = 0;
    virtual ~IOrderBookObserver() = default;
};

class Logger final : public IOrderBookObserver {
public:
    void onOrderBookUpdate() override {
        // std::cout << "OrderBook updated\n";
    }
    ~Logger() override = default;
};

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
    OrderExecutionType exection_type;
    TimeInForce time_in_force;

    f32 price;
    u32 quantity;
    u64 timestamp_ns;
    bool has_price;

    Order() = default;

    Order(u64 order_reference_id, OrderSide side, OrderExecutionType execution_type, TimeInForce time_in_force, f32 price, u32 quantity, u64 timestamp_ns)
        : order_reference_id(order_reference_id), side(side), exection_type(execution_type), time_in_force(time_in_force), price(price), quantity(quantity), timestamp_ns(timestamp_ns), has_price(execution_type == OrderExecutionType::LIMIT) {}

    friend std::ostream& operator<<(std::ostream& os, const Order& ord) {
            os << "Order(id=" << ord.order_reference_id
                << ", side=" << to_underlying(ord.side)
                << ", policy=" << to_underlying(ord.exection_type)
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
    
    class Builder {
        u32 id_ {0};
        OrderSide side_ {OrderSide::BUY};
        OrderExecutionType execution_type_ {OrderExecutionType::MARKET};
        TimeInForce time_in_force_ {TimeInForce::GTC};
        f32 price_ {0};
        u32 quantity_ {0};
        u64 timestamp_ns_ {0};
        bool has_price {false};

    public:
        Builder& setTimestamp(u64 timestamp_ns) { timestamp_ns_ = timestamp_ns; return *this; }
        Builder& setId(u64 id) { id_ = id; return *this; }

        Builder& setSide(std::byte side) {
            switch(static_cast<char>(side)) {
                case 'B' : side_ = OrderSide::BUY; break;
                case 'S' : side_ = OrderSide::SELL; break;
            }
            return *this;
        }

        Builder& setExecutionType(OrderExecutionType execution_type) { execution_type_ = execution_type; return *this;}
        Builder& setTimeInForce(TimeInForce time_in_force) { time_in_force_ = time_in_force; return *this; }
        Builder& setPrice(f32 price) { price_ = price; has_price = true; return *this; }
        Builder& setQuantity(u32 quantity) { quantity_ = quantity; return *this; }

        Order build() {
            if (execution_type_ == OrderExecutionType::MARKET && has_price) {
                throw std::runtime_error("Market orders should not have a price");
            }
            
            if (execution_type_ == OrderExecutionType::LIMIT && !has_price){
                throw std::runtime_error("Limit orders require a price");
            }
            
            if (quantity_ == 0) {
                throw std::runtime_error("Quantity must be greater than zero");
            }

            return Order(id_, side_, execution_type_, time_in_force_, price_, quantity_, timestamp_ns_);
        }
    };
};

using OrderMessage = std::variant<
    std::monostate,
    AddOrderNoMPIDMessage,
    OrderDeleteMessage,
    OrderCancelMessage,
    OrderExecutedMessage
>;

class OrderBook {  
    std::vector<IOrderBookObserver*> observers;
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

public:    
    OrderBook() : message_queue(10000) {} 

    OrderBook(const std::string& sym, f32 ts = 0.01)
        : tick_size(ts), message_queue(10000) {
        std::snprintf(symbol, sizeof(symbol), "%s", sym.c_str());
    }

    void submitMessage(const OrderMessage& message) {
        std::ignore = message_queue.try_push(message);
    }

    void start() {
        running = true;
        processing_thread = std::thread(&OrderBook::processMessages, this);
    }

    void stop() {
        running = false;
        if(processing_thread.joinable()) {
            processing_thread.join();
        } 
    }

    void processMessages() {
        while(running) {
            OrderMessage* msg = message_queue.front();
        
            if(!std::holds_alternative<std::monostate>(*msg)) {
                processMessage(*msg);
            }

            message_queue.pop();
        }

        while(true) {
            OrderMessage* msg = message_queue.front();
            if(msg == nullptr) break;
        
            processMessage(*msg);
            message_queue.pop();
        }
    }


    void processMessage(const OrderMessage& msg) {
        std::visit(overloaded {
            [this] (const AddOrderNoMPIDMessage& msg) {
                Order order = Order::Builder()
                    .setTimestamp(msg.header.timestamp)
                    .setId(msg.order_reference_number)
                    .setSide(msg.buy_sell_indicator)
                    .setExecutionType(OrderExecutionType::LIMIT)
                    .setTimeInForce(TimeInForce::GTC)
                    .setPrice(msg.price)
                    .setQuantity(msg.shares)
                    .build();

                addOrder(order);
            },
            [this] (const OrderDeleteMessage& msg) {    
                removeOrderFromId(msg.order_reference_number);
            },
            [this] (const OrderCancelMessage& msg) {
                cancelOrder(msg.order_reference_number, msg.cancelled_shares);
            },
            [this] (const OrderExecutedMessage& msg) {
                executeOrder(msg.order_reference_number, msg.executed_shares, msg.match_number);
            },
            [this] (const std::monostate) {}
        }, msg);

        print();
    }
               
    bool addObserver(IOrderBookObserver* obs) {
        observers.push_back(obs);
        observer_count++;
        return true;
    }

    bool removeObserver(IOrderBookObserver* obs) {
        auto it = std::find(observers.begin(), observers.end(), obs);
        if (it != observers.end()) {
            observers.erase(it);
            return true;
        }
        return false;
    }
    
    void notify() {
        for (size_t i = 0; i < observer_count; ++i) {
            observers[i]->onOrderBookUpdate();
        }
    }

    void addOrder(const Order& order) {
        if (!order.has_price) {
            notify();
            return;
        }

        orders[order.order_reference_id] = order;
        
        if (order.side == OrderSide::BUY) {
            bids[order.price].push_back(order.order_reference_id);
        } else {
            asks[order.price].push_back(order.order_reference_id);
        }

        notify();
    }

    Order& getOrderFromId(u64 order_id) {
        return orders.at(order_id);
    }

    Order removeOrderFromId(u64 order_id) {
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

    void cancelOrder(u64 order_id, u32 cancelled_shares) {
        Order& order = getOrderFromId(order_id);
        order.quantity -= cancelled_shares;
        
        if (order.quantity == 0) {
            removeOrderFromId(order_id);
        }       
    }   

    void executeOrder(u64 order_id, u32 executed_shares, u64 match_order_id) {
        Order& order = getOrderFromId(order_id);
        order.quantity -= executed_shares;

        Order& match_order = getOrderFromId(match_order_id);
        match_order.quantity -= executed_shares;
       
        if (order.quantity == 0) {
            removeOrderFromId(order_id);
        }

        if(match_order.quantity == 0) {
            removeOrderFromId(match_order_id);
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
 
    const char* getSymbol() const noexcept { return symbol; }
    f32 getTickSize() const noexcept { return tick_size; }
    size_t getObserverCount() const noexcept { return observer_count; }
};

constexpr size_t get_message_size(std::byte c) {
    switch (static_cast<char>(c)) {
        case 'A': return sizeof(AddOrderNoMPIDMessage);
        case 'D': return sizeof(OrderDeleteMessage);
        case 'X': return sizeof(OrderCancelMessage);
        case 'E': return sizeof(OrderExecutedMessage);
        default: return 0;
    }
}

void edit_book(const std::byte* ptr, size_t size, OrderBook& ob) {
    const std::byte* end = ptr + size;

    while (ptr < end) {
        std::byte type = *ptr;
        size_t msg_size = get_message_size(type);
        if (msg_size == 0 || ptr + msg_size > end) break;

        switch(static_cast<char>(type)) {
            case 'A' : {
                const AddOrderNoMPIDMessage* msg = reinterpret_cast<const AddOrderNoMPIDMessage*>(ptr);

                Order order = Order::Builder()
                    .setTimestamp(msg->header.timestamp)
                    .setId(msg->order_reference_number)
                    .setSide(msg->buy_sell_indicator)
                    .setExecutionType(OrderExecutionType::LIMIT)
                    .setTimeInForce(TimeInForce::GTC)
                    .setPrice(msg->price)
                    .setQuantity(msg->shares)
                    .build();

                ob.addOrder(order);
                
                break;
            }

            case 'D' : {
                const OrderDeleteMessage* msg = reinterpret_cast<const OrderDeleteMessage*>(ptr);    

                ob.removeOrderFromId(msg->order_reference_number);

                break;
            }

            case 'X' : {
                const OrderCancelMessage* msg = reinterpret_cast<const OrderCancelMessage*>(ptr);    

                ob.cancelOrder(msg->order_reference_number, msg->cancelled_shares);

                break;
            }

            case 'E' : {
                const OrderExecutedMessage* msg = reinterpret_cast<const OrderExecutedMessage*>(ptr);    

                ob.executeOrder(msg->order_reference_number, msg->executed_shares, msg->match_number);

                break;
            }
        }

        #if DEBUG
            ob.print();
        #endif

        ptr += msg_size;
    }
}

// void test() 
// {
//  assert(1 == 1);
// }


int main() {
    auto ob = OrderBook("AAPL");

    Logger logger;
    ob.addObserver(&logger);
   
    ob.start();

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

    OrderDeleteMessage cancel_order = {
        .header = {
            .message_type = 'D',
            .stock_locate = 0,
            .tracking_number = 0,
            .timestamp = 1
        },
        .order_reference_number = 1,
    };

    ob.submitMessage(add_order);

    ob.submitMessage(cancel_order);

    ob.stop();

    return 0;
}
