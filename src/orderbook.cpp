#include <stdexcept>

#include "util.hpp"
#include "orderbook.hpp"

constexpr static std::byte BUY_BYTE = static_cast<std::byte>('B');

#define DEBUG 0

std::ostream& operator<<(std::ostream& os, const Order& ord) {
    os << "Order(id=" << ord.order_reference_id
       << ", side=" << static_cast<char>(ord.side)
       << ", policy=" << std::to_underlying(ord.execution_type)
       << ", time_in_force=" << std::to_underlying(ord.time_in_force)
       << ", price=" << (ord.has_price ? std::to_string(ord.price) : "market")
       << ", quantity=" << ord.quantity
       << ", timestamp=" << ord.timestamp_ns << "ns"
       << ")";
    return os;
}

OrderBook::OrderBook() : message_queue(10000), last_order_id(0) {
    start();
} 

OrderBook::OrderBook(const std::string& sym, f32 ts)
    : tick_size(ts), message_queue(10000), last_order_id(0) {
    symbol = sym;
    start();
}

OrderBook::~OrderBook() {
    stop();
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
    if(!message_queue.try_push(message)) {
        throw std::runtime_error("Failed to push order to Message Queue");
    }
    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
}

void OrderBook::process_messages() {
    while(running) {
        OrderMessage* msg = message_queue.front();

        if(msg == nullptr) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            continue;
        }
        
        message_queue.pop();

        if(!std::holds_alternative<std::monostate>(*msg)) {
            process_message(*msg);
        }
    }
}

void OrderBook::process_message(const OrderMessage& msg) {
    std::visit(overloaded {
        [this] (const Order& order) {
            add_order_to_book(order);            
        },
        [this](const auto& msg) requires (
            std::same_as<std::decay_t<decltype(msg)>, AddOrderWithMPIDMessage> || // for now ignore MPID
            std::same_as<std::decay_t<decltype(msg)>, AddOrderNoMPIDMessage>
        ) {
            if(msg.stock != get_symbol()) {
                throw std::runtime_error("AddOrderNoMPIDMessage/AddOrderWithMPIDMessage Stock/Symbol failed to match OrderBook Symbol field");
            }

            Order order {
                .order_reference_id = msg.order_reference_number,
                .side = msg.buy_sell_indicator,
                .execution_type = OrderExecutionType::LIMIT,
                .time_in_force = TimeInForce::GTC,
                .price = msg.price,
                .quantity = msg.shares,
                .timestamp_ns = msg.header.timestamp,
                .has_price = true
            };            

            add_order_to_book(order);
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
        [this] (const OrderExecutedwithPriceMessage& msg) { // todo, do smt with executed with price its a liite different
            execute_order(msg.order_reference_number, msg.executed_shares, msg.match_number);
        }, 
        [this] (const OrderReplaceMessage& msg) {
            replace_order(msg.original_order_reference_number, msg.new_order_reference_number, msg.shares, msg.price);  
        },
        // Since Trade Messages do not affect the book,
        // however, they may be ignored by firms just looking to
        // build and track the Nasdaq execution system
        [this] (const TradeMessage& msg) {},

        // TODO
        [this] (const StockDirectoryMessage& msg) {}, // just metadata
        [this] (const StockTradingActionMessage& msg) {},
        [this] (const SystemEventMessage& msg) {},
        [this] (const CrossTradeMessage& msg) {},               
        [this] (const BrokenTradeMessage& msg) {},
        [this] (const NOIIMessage& msg) {},
        [this] (const DirectListingWithCapitalRaisePriceMessage& msg) {},
        [this] (const MarketParticipantPositionMessage& msg) {},
        [this] (const ShortSalePriceTestMessage& msg) {},
        [this] (const MWCBDeclineLevelMessage& msg) {},
        [this] (const MWCBStatusMessage& msg) {},
        [this] (const QuotingPeriodUpdateMessage& msg) {},
        [this] (const LULDAuctionCollarMessage& msg) {},
        [this] (const OperationalHaltMessage& msg) {},

        [this] (const std::monostate) {} // default
    }, msg);

    #if DEBUG
        print();
    #endif
}

void OrderBook::add_order(f32 price, u32 quantity, char side) {
    // if hashmap has that order id ++ until it doesnt
    while (order_id_map.contains(last_order_id)) {
        ++last_order_id;
    }
        
    Order order = {
        .order_reference_id = last_order_id,
        .side = static_cast<std::byte>(side),
        .execution_type = OrderExecutionType::LIMIT,
        .time_in_force = TimeInForce::GTC,
        .price = price,
        .quantity = quantity,
        .timestamp_ns = get_ns_from_midnight(),
        .has_price = true        
    };

    // submit_message(order);
    add_order_to_book(order);
        
    #if DEBUG
        print();
    #endif
}

void OrderBook::add_order_to_book(const Order& order) {
    if (!order.has_price) {
        return;
    }

    order_id_map[order.order_reference_id] = order;
    
    if (order.side == BUY_BYTE) {
        bids[order.price].push_back(order.order_reference_id);
    } else {
        asks[order.price].push_back(order.order_reference_id);
    }
}


Order& OrderBook::get_order_from_id(u64 order_id) {
    return order_id_map.at(order_id);
}

Order OrderBook::remove_order_from_id(u64 order_id) {
    Order& order = get_order_from_id(order_id);
    auto& level = (order.side == BUY_BYTE) ? bids[order.price] : asks[order.price];
    level.erase(std::find(level.begin(), level.end(), order_id));
    
    if (level.empty()) {
        if (order.side == BUY_BYTE) {
            bids.erase(order.price);
        } else {
            asks.erase(order.price);
        }
    }
    
    order_id_map.erase(order_id);
    
    return order;
}

void OrderBook::cancel_order(u64 order_id, u32 cancelled_shares) {
    Order& order = get_order_from_id(order_id);
    order.quantity -= cancelled_shares;

    if (order.quantity == 0) {
        remove_order_from_id(order_id);
    }       
}   

void OrderBook::execute_order(u64 order_id, u32 executed_shares, u64 match_number) {
    Order& order = get_order_from_id(order_id);
    order.quantity -= executed_shares;

    if (order.quantity == 0) {
        remove_order_from_id(order_id);
    }
}

void OrderBook::replace_order(u64 original_order_id, u64 new_order_id, u32 shares, f32 price) {
    Order new_order = get_order_from_id(original_order_id); // take most of the original orders data

    new_order.order_reference_id = new_order_id;
    new_order.price = price;
        
    remove_order_from_id(original_order_id);

    add_order_to_book(new_order);
}

void OrderBook::print() const {
    std::cout << "--- BIDS ---" << std::endl;
    for (const auto& [price, order_ids] : bids) {
        std::cout << "Price " << price << ":" << std::endl;
        for (u64 order_id : order_ids) {
            std::cout << "  " << order_id_map.at(order_id) << std::endl;
        }
    }

    std::cout << "--- ASKS ---" << std::endl;
    for (const auto& [price, order_ids] : asks) {
        std::cout << "Price " << price << ":" << std::endl;
        for (u64 order_id : order_ids) {
            std::cout << "  " << order_id_map.at(order_id) << std::endl;
        }
    }
    std::cout << std::endl;
}

f32 OrderBook::get_best_bid() {
    return bids.empty() ? 0.0 : bids.begin()->first;
}

f32 OrderBook::get_best_ask() {
    return asks.empty() ? 0.0 : asks.begin()->first;
}

const std::string OrderBook::get_symbol() const {
    return symbol;
}

f32 OrderBook::get_tick_size() const {
    return tick_size;
}
