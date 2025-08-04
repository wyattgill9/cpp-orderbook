#include "orderbook.hpp"

int main() {
    auto ob = OrderBook("TSLA");
    
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
        .stock = "TSLA",
        .price = 0.01
    };

    ob.submit_message(add_order);

    ob.add_order(0.01, 1000, 'B', 1);

    ob.print();

    return 0;
}
