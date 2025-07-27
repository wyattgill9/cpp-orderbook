#include <cstdint>
#include <ostream>
#include <vector>
#include <array>
#include <print>
#include <iostream>
#include <cctype>
#include <string>
#include <algorithm>

#include "util.cpp"

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
    OrderExecutionType exection_type;
    TimeInForce time_in_force;

    float price;
    uint32_t quantity;
    uint64_t timestamp_ns;
    bool has_price;

    Order() = default;

    Order(uint32_t order_id, OrderSide side, OrderExecutionType execution_type, TimeInForce time_in_force, float price, uint32_t quantity)
        : order_id(order_id), side(side), exection_type(execution_type), time_in_force(time_in_force), price(price), quantity(quantity), timestamp_ns(getSysTime()), has_price(execution_type == OrderExecutionType::LIMIT) {}

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
        uint32_t id_ {0};

        OrderSide side_ = OrderSide::BUY; // default to buy order
        OrderExecutionType execution_type_ = OrderExecutionType::MARKET; // default to MARKET Order
        TimeInForce time_in_force_ = TimeInForce::GTC; // default to GTC order

        float price_ {0};
        uint32_t quantity_ {0};
        bool has_price = false;

    public:
        Builder& setId(uint64_t id) { id_ = id; return *this; }
        Builder& setSide(OrderSide side) { side_ = side; return *this; }
        Builder& setExecutionType(OrderExecutionType execution_type) { execution_type_ = execution_type; return *this;}
        Builder& setTimeInForce(TimeInForce time_in_force) { time_in_force_ = time_in_force; return *this; }
        Builder& setPrice(float price) { price_ = price; has_price = true; return *this; }
        Builder& setQuantity(uint32_t quantity) { quantity_ = quantity; return *this; }

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

            return Order(id_, side_, execution_type_, time_in_force_, price_, quantity_);
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
        std::cout << "OrderBook updated\n";
    }
    ~Logger() override = default;
};

// CORE ORDERBOOK
template<size_t MAX_ORDERS = 10000, size_t MAX_OBSERVERS = 10>
class OrderBook {
    std::vector<Order> orders;
    std::array<IOrderBookObserver*, MAX_OBSERVERS> observers;
    
    size_t order_count = 0;
    size_t observer_count = 0;

    // CONFIG 
    char symbol[8] = {0};
    double tick_size;

public:    
    OrderBook() {
        orders.reserve(MAX_ORDERS);
        observers.fill(nullptr);
    }
       
    OrderBook(const std::string& sym, double ts) : tick_size(ts) {
        orders.reserve(MAX_ORDERS);
        observers.fill(nullptr);
        
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
                for (size_t j = i; j < observer_count - 1; ++j) {
                    observers[j] = observers[j + 1];
                }
                observers[observer_count - 1] = nullptr;
                observer_count--;
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
        
        orders.push_back(order);
        order_count++;
        notify();
    }

    void print() const {
        for (const auto& order : orders) {
            std::cout << order << "\n";
        }
    }

    size_t size() const noexcept { return order_count; }
    bool full() const noexcept { return order_count >= MAX_ORDERS; }
    const char* getSymbol() const noexcept { return symbol; }
    double getTickSize() const noexcept { return tick_size; }
};

int main() {
    auto ob = OrderBook<10000, 5>("AAPL", 0.01);
    Logger logger;
    
    ob.addObserver(&logger);

    auto elapsed = time_ns([&]() {
        auto order1 = Order::Builder()
            .setId(1)
            .setSide(OrderSide::BUY)
            .setExecutionType(OrderExecutionType::MARKET)
            .setTimeInForce(TimeInForce::GTC)
            .setQuantity(100)
            .build();

        ob.addOrder(order1);                 
    });

    std::cout << "Elapsed: " << elapsed << " ns" << std::endl;

    ob.print();

    return 0;
}
