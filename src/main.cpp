#include "orderbook.hpp"
#include "messages.h"

int main() {
    auto ob = OrderBook("TSLA");
    ob.start();

    // std::byte buffer[1024];
    
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

    ob.submit_message(add_order);

    ob.stop();

    return 0;
}

// OrderDeleteMessage cancel_order = {
//     .header = {
//         .message_type = 'D',
//         .stock_locate = 0,
//         .tracking_number = 0,
//         .timestamp = 1
//     },
//     .order_reference_number = 1,
// };
//
// std::memcpy(buffer, &add_order, sizeof(add_order));
