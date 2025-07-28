#include <ostream>
#include <vector>
#include <array>
#include <print>
#include <iostream>
#include <cctype>
#include <string>
#include <algorithm>

#include "messages.cpp"

enum class OrderSide {
    BUY = 0, // B
    SELL = 1 // S
};

enum class OrderExecutionType {
    MARKET = 0,
    LIMIT = 1,
};

enum class TimeInForce {
    DAY = 0,
    GTC = 1, // Good Till Cancel
    IOC = 2, // Immediate Or Cancel
    FOK = 3  // Fill Or Kill
 };

// ORDER
struct Order {
    u64 order_id;

    OrderSide side;
    OrderExecutionType exection_type;
    TimeInForce time_in_force;

    f32 price;
    u32 quantity;
    u64 timestamp_ns;
    bool has_price;

    Order() = default;

    Order(u64 order_id, OrderSide side, OrderExecutionType execution_type, TimeInForce time_in_force, f32 price, u32 quantity, u64 timestamp_ns)
        : order_id(order_id), side(side), exection_type(execution_type), time_in_force(time_in_force), price(price), quantity(quantity), timestamp_ns(timestamp_ns), has_price(execution_type == OrderExecutionType::LIMIT) {}

    friend std::ostream& operator<<(std::ostream& os, const Order& ord) {
            os << "Order(id=" << ord.order_id
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

        OrderSide side_ {OrderSide::BUY}; // default to buy order
        OrderExecutionType execution_type_ {OrderExecutionType::MARKET}; // default to MARKET Order
        TimeInForce time_in_force_ {TimeInForce::GTC}; // default to GTC order

        f32 price_ {0};
        u32 quantity_ {0};
        u64 timestamp_ns_ {0};
        bool has_price {false};

    public:
        Builder& setTimestamp(u64 timestamp_ns) { timestamp_ns_ = timestamp_ns; return *this; }
        Builder& setId(u64 id) { id_ = id; return *this; }
        Builder& setSide(u8 side) {
            switch(side) {
                case 'B' : {
                    side_ = OrderSide::BUY;
                    break;
                }
                case 'S' : {
                    side_ = OrderSide::SELL;
                    break;        
                }
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

// OBSERVER PATTERN 
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

// CORE ORDERBOOK
template<size_t MAX_ORDERS = 10000, size_t MAX_OBSERVERS = 10>
class OrderBook {  
    std::array<Order, MAX_ORDERS> orders;
    std::array<IOrderBookObserver*, MAX_OBSERVERS> observers;
    
    size_t order_count {0};
    size_t observer_count {0};

    // CONFIG 
    char symbol[8] {0};
    f32 tick_size;

public:    
    OrderBook() = default;
       
    OrderBook(const std::string& sym, f32 ts) : tick_size(ts) {      
        size_t len = std::min(sym.length(), sizeof(symbol) - 1);
        std::copy_n(sym.c_str(), len, symbol);
        symbol[len] = '\0';
    }

    // OBSERVER PATTERN 
    bool addObserver(IOrderBookObserver* obs) {
        if (observer_count >= MAX_OBSERVERS || obs == nullptr) {
            return false;
        }
        
        observers[observer_count] = obs;
        observer_count++;
        return true;
    }

    bool removeObserver(IOrderBookObserver* obs) {
        for (size_t i = 0; i < observer_count; ++i) {
            if (observers[i] == obs) {
                --observer_count;
                if (i != observer_count) {
                    observers[i] = observers[observer_count];
                }
                observers[observer_count] = nullptr;
                return true;
            }
        }
        return false;
    }
    
    void notify() {
        for (size_t i = 0; i < observer_count; ++i) {
            observers[i]->onOrderBookUpdate();
        }
    }

    void addOrder(const Order& order) {
        if (order_count >= MAX_ORDERS) {
            throw std::runtime_error("OrderBook is full");
        }
        
        orders[order_count] = order;
        notify();
    }

    void print() const {
        for (size_t i = 0; i < order_count; ++i) {
            std::cout << orders[i] << "\n";
        }
    }

    size_t size() const noexcept { return order_count; }
    bool full() const noexcept { return order_count >= MAX_ORDERS; }
    const char* getSymbol() const noexcept { return symbol; }
    f32 getTickSize() const noexcept { return tick_size; }
    size_t getObserverCount() const noexcept { return observer_count; }
};

static constexpr size_t message_sizes[256] = {
    ['A'] = sizeof(AddOrderNoMPIDMessage),
    ['D'] = sizeof(OrderDeleteMessage),
    ['X'] = sizeof(OrderCancelMessage),
    ['E'] = sizeof(OrderExecutedMessage),
};

void edit_book(const uint8_t* ptr, size_t size, OrderBook<>& ob) {
    const uint8_t* end = ptr + size;

    while (ptr < end) {
        uint8_t type = *ptr;
        size_t msg_size = message_sizes[type];
        if (msg_size == 0 || ptr + msg_size > end) break;

        switch(type) {
            case 'A' : {
                const auto* msg = reinterpret_cast<const AddOrderNoMPIDMessage*>(ptr);

                // ASSUMING ALL INCOMING ORDERS ARE FOR THE SAME SECURITY/PAIR
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
                const auto* msg = reinterpret_cast<const OrderDeleteMessage*>(ptr);    
                break;
            }
            case 'X' : {
                const auto* msg = reinterpret_cast<const OrderCancelMessage*>(ptr);    
                break;
            }
            case 'E' : {
                const auto* msg = reinterpret_cast<const OrderCancelMessage*>(ptr);    
                break;
            }
        }
   
        ptr += msg_size;
    }
}

int main() {
    auto ob = OrderBook<>("AAPL", 0.01);

    Logger logger;
    ob.addObserver(&logger);

    u8 buffer[64];
    
    AddOrderNoMPIDMessage a = {
        .header = {
            .message_type = 'A',
            .stock_locate = 0,
            .tracking_number = 0,
            .timestamp = 1
        },
        .order_reference_number = 1,
        .buy_sell_indicator = 'B',
        .shares = 1000,
        .stock = "AAPL",
        .price = 100
    };

    std::memcpy(buffer, &a, sizeof(a));

    auto elapsed = time_ns([&] {
        edit_book(buffer, 64, ob);
    });
    
    std::cout << "Elapsed: " << elapsed << std::endl;

    ob.print();

    return 0;
}
