#include <cstdint>
#include <memory>
#include <ostream>
#include <vector>
#include <print>
#include <iostream>
#include <type_traits>
#include <cctype>
#include <chrono>
#include <string>
#include <optional>

// UTIL
template <typename Enum>
constexpr auto to_underlying(Enum e) {
    return static_cast<std::underlying_type_t<Enum>>(e);
}

// gets system time in ns
uint64_t getSysTime() {
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    return ns.time_since_epoch().count();
}

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
    OrderExecutionType exection_type;
    TimeInForce time_in_force;

    std::optional<float> price;
    uint32_t quantity;
    uint64_t timestamp_ns;

    Order() = default;

    Order(uint32_t order_id, OrderSide side, OrderExecutionType execution_type, TimeInForce time_in_force, std::optional<float> price, uint32_t quantity, uint64_t timestamp_ns)
        : order_id(order_id), side(side), exection_type(execution_type), time_in_force(time_in_force), price(price), quantity(quantity), timestamp_ns(timestamp_ns){}

    friend std::ostream& operator<<(std::ostream& os, const Order& ord) {
            os << "Order(id=" << ord.order_id
                << ", side=" << to_underlying(ord.side)
                << ", policy=" << to_underlying(ord.exection_type)
                << ", time_in_force=" << to_underlying(ord.time_in_force)
                << ", price=";
            if(ord.price.has_value())
                os << *ord.price;
            else
                os << "nullopt";

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

        std::optional<float> price_ {std::nullopt};
        uint32_t quantity_ {0};

    public:
        Builder& setId(uint64_t id) { id_ = id; return *this; }
        Builder& setSide(OrderSide side) { side_ = side; return *this; }
        Builder& setExecutionType(OrderExecutionType execution_type) { execution_type_ = execution_type; return *this;}
        Builder& setTimeInForce(TimeInForce time_in_force) { time_in_force_ = time_in_force; return *this; }
        Builder& setPrice(float price) { price_ = price; return *this; }
        Builder& setQuantity(uint32_t quantity) { quantity_ = quantity; return *this; }

        Order build() {
            if (execution_type_ == OrderExecutionType::MARKET && price_.has_value()) {
                throw std::runtime_error("Market orders should not have a price");
            }
            
            if (execution_type_ == OrderExecutionType::LIMIT && !price_.has_value()) {
                throw std::runtime_error("Limit orders require a price");
            }
            
            if (quantity_ == 0) {
                throw std::runtime_error("Quantity must be greater than zero");
            }

            return Order(id_, side_, execution_type_, time_in_force_, price_, quantity_, getSysTime());
        }
    };
};

// OBSERVER PATTER
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
class OrderBook {
    std::vector<Order> orders;
    std::vector<std::shared_ptr<IOrderBookObserver>> observers;

    // CONFIG
    std::string symbol;
    double tick_size;
    uint32_t max_orders;

public:    
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;
    ~OrderBook() = default;

    OrderBook() = default;

    OrderBook(std::string symbol_, double tick_size_, uint32_t max_orders_)
        : symbol(symbol_), tick_size(tick_size_), max_orders(max_orders_) {}

    // OBSERVER PATTERN
    // L-value existing adresses
    void addObserver(std::shared_ptr<IOrderBookObserver>& obs) {
        observers.push_back(std::move(obs));
    }

    // R-value of obs
    void addObserver(std::shared_ptr<IOrderBookObserver>&& obs) {
        observers.push_back(std::move(obs));
    }

    void notify() {
        for(auto& obs: observers) {
            obs->onOrderBookUpdate();
        }
    }

    void addOrder(const Order& order) {
        orders.push_back(order);
        notify();
    }

    void print() {
        for(const auto& order : orders) {
            std::cout << order << " " << std::endl;
        }
    }

    class Builder {
        std::string symbol = "";
        double tick_size = 0.01;
        uint32_t max_orders = 10000;

    public:
        Builder& setSymbol(std::string symbol_) { symbol = symbol_; return *this; }
        Builder& setTickSize(double tick_size_) { tick_size = tick_size_; return *this; }
        Builder& setMaxOrders(uint32_t max_orders_) { max_orders = max_orders_; return *this;} 
        
        std::shared_ptr<OrderBook> build() {
            return std::make_shared<OrderBook>(symbol, tick_size, max_orders);
        }
    };
};

auto main() -> int {
    std::shared_ptr<OrderBook> ob = OrderBook::Builder()
        .setSymbol("AAPL")
        .build();
              
    auto logger = std::make_shared<Logger>();

    ob->addObserver(logger);

    auto Order1 = Order::Builder()
        .setId(1)
        .setSide(OrderSide::BUY)
        .setExecutionType(OrderExecutionType::MARKET)
        .setTimeInForce(TimeInForce::GTC)
        .setQuantity(100)
        .build();

    ob->addOrder(Order1);
    ob->print();

    return 0;
}
