#include <algorithm>
#include <utility>

#include "util.hpp"
#include "orderbook.hpp"

OrderSide char_to_order_side(std::byte indicator);

std::ostream& operator<<(std::ostream& os, const Order& ord) {
    os << "Order(id=" << ord.order_reference_id
       << ", side=" << std::to_underlying(ord.side)
       << ", policy=" << std::to_underlying(ord.execution_type)
       << ", time_in_force=" << std::to_underlying(ord.time_in_force)
       << ", price=" << (ord.has_price ? std::to_string(ord.price) : "market")
       << ", quantity=" << ord.quantity
       << ", timestamp=" << ord.timestamp_ns << "ns"
       << ")";
    return os;
}

OrderBook::OrderBook() : message_queue(10000) {} 

OrderBook::OrderBook(const std::string& sym, f32 ts)
    : tick_size(ts), message_queue(10000) {
    std::snprintf(symbol, sizeof(symbol), "%s", sym.c_str());
}

void OrderBook::start() {
    running = true;
    processing_thread = std::thread(&OrderBook::process_messages, this);
}

void OrderBook::stop() {
    running = false;
    if(processing_thread.joinable()) {
        processing_thread.join();
    } 
}

void OrderBook::submit_message(const OrderMessage& message) {
    std::ignore = message_queue.try_push(message);
}

void OrderBook::process_messages() {
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

void OrderBook::process_message(const OrderMessage& msg) {
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

void OrderBook::add_order(const Order& order) {
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

Order& OrderBook::get_order_from_id(u64 order_id) {
    return orders.at(order_id);
}

Order OrderBook::remove_order_from_id(u64 order_id) {
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

void OrderBook::cancel_order(u64 order_id, u32 cancelled_shares) {
    Order& order = get_order_from_id(order_id);
    order.quantity -= cancelled_shares;

    if (order.quantity == 0) {
        remove_order_from_id(order_id);
    }       
}   

void OrderBook::execute_order(u64 order_id, u32 executed_shares, u64 match_order_id) {
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

void OrderBook::edit_book(const std::byte* ptr, size_t size) {
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

        ptr += msg_size;
    }
}

void OrderBook::print() const {
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

f32 OrderBook::get_best_bid() {
    return bids.begin()->first;
}

f32 OrderBook::get_best_ask() {
    return asks.begin()->first;
}

const char* OrderBook::get_symbol() const {
    return symbol;
}

f32 OrderBook::get_tick_size() const {
    return tick_size;
}

OrderSide char_to_order_side(std::byte indicator) {
    return (static_cast<char>(indicator) == 'B') ? OrderSide::BUY : OrderSide::SELL;
}
