#include <cstdint>
#include <ostream>
#include <vector>
#include <print>
#include <iostream>
#include <cctype>
#include <string>
#include <array>

#include "util.cpp"
#include "messages.cpp"

enum class ErrorCode {
    SUCCESS,
    INVALID_PRICE_FOR_MARKET_ORDER
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
    GTC = 1, // Good Till Cancel
    IOC = 2, // Immediate Or Cancel
    FOK = 3  // Fill Or Kill
};

// ORDER
struct Order {
    uint64_t order_id;

    OrderSide side;
    OrderExecutionType execution_type;
    TimeInForce time_in_force;

    float price;
    bool has_price;
    uint32_t quantity;
    uint64_t timestamp_ns;

    Order() = default;

    Order(uint32_t order_id, OrderSide side, OrderExecutionType execution_type, TimeInForce time_in_force, float price, bool has_price, uint32_t quantity, uint64_t timestamp_ns)
        : order_id(order_id), side(side), execution_type(execution_type), time_in_force(time_in_force), price(price), has_price(has_price), quantity(quantity), timestamp_ns(timestamp_ns){}

    friend std::ostream& operator<<(std::ostream& os, const Order& ord) {
        os << "Order(id=" << ord.order_id
           << ", side=" << to_underlying(ord.side)
           << ", policy=" << to_underlying(ord.execution_type)
           << ", time_in_force=" << to_underlying(ord.time_in_force)
           << ", price=";
        if(ord.has_price)
            os << ord.price;
        else
            os << "MARKET";

        os << ", quantity=" << ord.quantity
           << ", timestamp=" << ord.timestamp_ns << "ns" 
           << ")";
        return os;
    }
    
    class Builder {
        uint32_t id_ {0};

        OrderSide side_ = OrderSide::BUY; // default to buy order
        OrderExecutionType execution_type_ = OrderExecutionType::MARKET; // default to MARKET Order
        TimeInForce time_in_force_ = TimeInForce::GTC; // default to GTC order

        float price_;
        bool has_price_ {false};

        uint32_t quantity_ {0};

    public:
        Builder& setId(uint64_t id) { id_ = id; return *this; }
        Builder& setSide(OrderSide side) { side_ = side; return *this; }
        Builder& setExecutionType(OrderExecutionType execution_type) { execution_type_ = execution_type; return *this;}
        Builder& setTimeInForce(TimeInForce time_in_force) { time_in_force_ = time_in_force; return *this; }
        Builder& setPrice(float price) { price_ = price; has_price_ = true; return *this; }
        Builder& setQuantity(uint32_t quantity) { quantity_ = quantity; return *this; }

        Order build() {
            if (execution_type_ == OrderExecutionType::MARKET && has_price_) {
                throw std::runtime_error("Market orders should not have a price");
            }
            
            if (execution_type_ == OrderExecutionType::LIMIT && !has_price_) {
                throw std::runtime_error("Limit orders require a price");
            }
            
            if (quantity_ == 0) {
                throw std::runtime_error("Quantity must be greater than zero");
            }

            return Order(id_, side_, execution_type_, time_in_force_, price_, has_price_, quantity_, getSysTime());
        }
    };
};

// OBSERVER PATTERN
class IOrderBookObserver {
public:
    virtual void onOrderBookUpdate() = 0;
    virtual ~IOrderBookObserver() = default;
};

class Logger : public IOrderBookObserver {
public:
    void onOrderBookUpdate() override {
        std::cout << "OrderBook updated\n";
    }
    ~Logger() override = default;
};

// CORE ORDERBOOK
template<int MAX_ORDERS = 10000, int MAX_OBSERVERS = 10>
class OrderBook {
    std::vector<Order> orders;
    std::array<IOrderBookObserver*, MAX_OBSERVERS> observers;

    size_t observer_count;
    size_t order_count;

    // CONFIG
    std::string symbol;
    float tick_size;

public:    
    ~OrderBook() = default;

    OrderBook(const std::string& symbol_, float tick_size_)
        : symbol(symbol_), tick_size(tick_size_), observer_count(0), order_count(0) {
        orders.reserve(MAX_ORDERS);
        observers.fill(nullptr);
    }

    // OBSERVER PATTERN
    void addObserver(IOrderBookObserver* obs) {
        if (observer_count < MAX_OBSERVERS && obs != nullptr) {
            observers[observer_count] = obs;
            observer_count++;
        }
    }

    void notify() {
        for(size_t i = 0; i < observer_count; ++i) {
            if (observers[i] != nullptr) {
                observers[i]->onOrderBookUpdate();
            }
        }
    }

    void addOrder(const Order& order) {
        if (order_count < MAX_ORDERS) {
            orders.push_back(order);
            order_count++;
            notify();
        }
    }

    void print() const {
        for(const auto& order : orders) {
            std::cout << order << " " << std::endl;
        }
    }

    size_t getObserverCount() { return observer_count;}
    size_t getOrderCount() { return order_count; }
};

int main() {
    auto ob = OrderBook<>("AAPL", 0.01);   
    Logger logger;

    ob.addObserver(&logger);

    auto order1 = Order::Builder()
        .setId(1)
        .setSide(OrderSide::BUY)
        .setExecutionType(OrderExecutionType::MARKET)
        .setTimeInForce(TimeInForce::GTC)
        .setQuantity(100)
        .build();

    ob.addOrder(order1);
    ob.print();

    return 0;
}
