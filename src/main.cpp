#include <memory>
#include <ostream>
#include <vector>
#include <print>
#include <iostream>
#include <type_traits>

template <typename Enum>
constexpr auto to_underlying(Enum e) {
    return static_cast<std::underlying_type_t<Enum>>(e);
}

// BUY/SELL
enum class OrderType { BUY, SELL };

// ORDER
struct Order {
    int id;
    OrderType type;
    double price;
    int quantity;

    Order() = default;

    Order(int id, OrderType type, double price, int quantity)
        : id(id), type(type), price(price), quantity(quantity) {}

    friend std::ostream& operator<<(std::ostream& os, const Order& ord) {
        os  << "Order(id=" << ord.id
            << ", type=" << to_underlying(ord.type)
            << ", price=" << ord.price
            << ", quantity=" << ord.quantity
            << ")";
        return os;
    }
};

// BUILDER PATTERN
class OrderBuilder {
    int id = 0;
    OrderType type = OrderType::BUY;
    double price = 0;
    int quantity = 0;

public:
    OrderBuilder& setId(int id_) { id = id_; return *this; }
    OrderBuilder& setType(OrderType type_) { type = type_; return *this; }
    OrderBuilder& setPrice(double price_) { price = price_; return *this; }
    OrderBuilder& setQuantity(int quantity_) { quantity = quantity_; return *this; }

    Order build() {
        return Order(id, type, price, quantity);
    }
};

class IOrderBookObserver {
public:
    virtual void onOrderBookUpdate() = 0;
    virtual ~IOrderBookObserver() = default;
};

// CORE ORDERBOOK
class OrderBook {
    std::vector<Order> orders;
    std::vector<std::unique_ptr<IOrderBookObserver>> observers;
    
    OrderBook() {}

public:    
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;

    static OrderBook& getInstance() {
        static OrderBook instance;
        return instance;
    }
    
    // OBSERVER PATTERN
    void addObserver(std::unique_ptr<IOrderBookObserver> obs) {
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
}; 

class Logger : public IOrderBookObserver {
public:
    void onOrderBookUpdate() override {
        std::cout << "OrderBook updated\n";
    }
    ~Logger() override = default;
};

int main() {
    OrderBook& ob = OrderBook::getInstance();

    ob.addObserver(std::make_unique<Logger>());

    auto Order1 = OrderBuilder()
        .setId(1)
        .setType(OrderType::BUY)
        .setPrice(100)
        .setQuantity(100)
        .build();

    ob.addOrder(Order1);
    ob.print();

    return 0;
}
