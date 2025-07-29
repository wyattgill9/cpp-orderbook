#include <ostream>
#include <array>
#include <print>
#include <iostream>
#include <cctype>
#include <pthread.h>
#include <string>
#include <algorithm>
#include <cstring>
#include <vector>

#include "messages.cpp"

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

template <size_t N>
struct FixedRingBuffer {
    Order data[N];
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;

    bool push_back(const Order& order) {
        if (count == N) return false; // full
        data[tail] = order;
        tail = (tail + 1) % N;
        ++count;
        return true;
    }

    bool pop_front() {
        if (count == 0) return false;
        head = (head + 1) % N;
        --count;
        return true;
    }

    Order& front() {
        return data[head];
    }

    const Order& front() const {
        return data[head];
    }

    // iterator
    struct Iterator {
        const FixedRingBuffer* buffer;
        size_t index;
        size_t remaining;

        Iterator(const FixedRingBuffer* buf, size_t idx, size_t rem)
            : buffer(buf), index(idx), remaining(rem) {}

        const Order& operator*() const {
            return buffer->data[index];
        }

        Iterator& operator++() {
            index = (index + 1) % N;
            --remaining;
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return remaining != other.remaining;
        }
    };

    Iterator begin() const {
        return Iterator(this, head, count);
    }

    Iterator end() const {
        return Iterator(this, 0, 0);
    }
};

struct PriceLevel {
    static constexpr size_t MAX_ORDERS = 1000;
    FixedRingBuffer<MAX_ORDERS> orders;

    f32 price {0.0f};
    bool active {false};

    void addOrder(const Order& order) {
        orders.push_back(order);
    }

    void popFront() {
        orders.pop_front();
    }

    void print() const {
        for(auto& order : orders) {
            std::cout << order << std::endl;
        }
    }
};

// CORE ORDERBOOK
template<size_t MAX_PRICE_LEVELS = 100>
class OrderBook {  
    std::vector<IOrderBookObserver*> observers;
    size_t observer_count {0};

    std::array<PriceLevel, MAX_PRICE_LEVELS> bids; // the highest price a buyer is currently willing to pay
    std::array<PriceLevel, MAX_PRICE_LEVELS> asks; // the lowest price a seller is willing to accept

    // CONFIG 
    char symbol[8] {0};
    f32 tick_size;
    f32 base_price;

    size_t priceToIndex(f32 price) const {
        return static_cast<size_t>((price - base_price) / tick_size);
    }

public:    
    OrderBook() = default;
      
    OrderBook(const std::string& sym, f32 ts = 0.01, f32 base_price = 0.0f)
        : tick_size(ts), base_price(base_price) {
        // symbol
        std::snprintf(symbol, sizeof(symbol), "%s", sym.c_str());

        // init array
        for (size_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
            bids[i].price = base_price + (i * tick_size);
            bids[i].active = false;

            asks[i].price = base_price + (i * tick_size);
            asks[i].active = false;
        }
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
            // handle market order
            notify();
            return;
        }

        size_t price_index = priceToIndex(order.price);

        switch (order.side) {
            case OrderSide::BUY: {
                PriceLevel& level = bids[price_index];
                level.addOrder(order);
                level.active = true;
                break;
            }
            case OrderSide::SELL: {
                PriceLevel& level = asks[price_index];
                level.addOrder(order);
                level.active = true;
                break;
            }
            default:
                break;
        }

        notify();
    }

    void print() const {
        for (size_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
            if (bids[i].active)
                bids[i].print();
        }

        for (size_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
            if (asks[i].active)
                asks[i].print();
        }
    }
    
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
    auto ob = OrderBook<>("AAPL");

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
        .price = 0.01
    };

    std::memcpy(buffer, &a, sizeof(a));

    auto elapsed = time_ns([&] {
        edit_book(buffer, 64, ob);
    });
    
    std::cout << "Elapsed: " << elapsed << std::endl;

    ob.print();

    return 0;
}
