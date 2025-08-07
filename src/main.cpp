#include "orderbook.hpp"

int main() {
    auto ob = OrderBook("TSLA");

    // You can use ITCH standard protocol structs:
    AddOrderNoMPIDMessage add_order = {
        .header = {
            .message_type = 'A',
            .stock_locate = 0,
            .tracking_number = 0,
            .timestamp = 1
        },
        .order_reference_number = 0,
        .buy_sell_indicator = static_cast<std::byte>('B'),
        .shares = 1000,
        .stock = "TSLA",
        .price = 0.01
    };

    ob.submit_message(add_order);

    // OR just add orders like you would normally

    ob.add_order(0.01, 100, 'B'); 

    // for(int price = 1; price <= 1000; price++) {
    //     for(int quantity = 1; quantity <= 1000; quantity++) {
    //         ob.add_order(price, quantity, 'B'); 
    //     }
    // }
    
    // OR make a buffer of multiple ITCH Messages and edit the book acordingly
    std::byte buffer[1024];

    OrderCancelMessage cancel_order = {
         .header = {
            .message_type = 'X',
            .stock_locate = 0,
            .tracking_number = 0,
            .timestamp = 2
        },
        .order_reference_number = 0,
        .cancelled_shares = 500
    };

    std::memcpy(buffer, &cancel_order, sizeof(cancel_order));
    ob.edit_book(buffer, sizeof(buffer));

    // Finally you can print out the OrderBook
    ob.print();

    return 0;
}
