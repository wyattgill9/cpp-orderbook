#include "orderbook.hpp"
#include "util.hpp"

void OrderBook::edit_book(const std::byte* ptr, size_t size) {
    const std::byte* end = ptr + size;

    while (ptr < end) {
        char type = static_cast<char>(*ptr);
        size_t msg_size = get_message_size(type);
        if (msg_size == 0 || ptr + msg_size > end) break;

        switch(type) {
            case 'A' : {
                const AddOrderNoMPIDMessage* msg = reinterpret_cast<const AddOrderNoMPIDMessage*>(ptr);

                if(msg->stock != get_symbol()) {
                    throw std::runtime_error("AddOrderNoMPIDMessage Stock/Symbol failed to match OrderBook Symbol field");
                }

                Order order {
                    .order_reference_id = msg->order_reference_number,
                    .side = msg->buy_sell_indicator,
                    .execution_type = OrderExecutionType::LIMIT,
                    .time_in_force = TimeInForce::GTC,
                    .price = msg->price,
                    .quantity = msg->shares,
                    .timestamp_ns = msg->header.timestamp,
                    .has_price = true 
                };

                add_order_to_book(order);

                break;
            }
            case 'D' : {
                const OrderDeleteMessage* msg = reinterpret_cast<const OrderDeleteMessage*>(ptr);    
                remove_order_from_id(msg->order_reference_number);
                break;
            }
            case 'X' : {
                const OrderCancelMessage* msg = reinterpret_cast<const OrderCancelMessage*>(ptr);    
                cancel_order(msg->order_reference_number, msg->cancelled_shares);
                break;
            }
            case 'E' : {
                const OrderExecutedMessage* msg = reinterpret_cast<const OrderExecutedMessage*>(ptr);    
                execute_order(msg->order_reference_number, msg->executed_shares, msg->match_number);
                break;
            }
            case 'U' : {
                const OrderReplaceMessage* msg = reinterpret_cast<const OrderReplaceMessage*>(ptr);    
                replace_order(msg->original_order_reference_number, msg->new_order_reference_number, msg->shares, msg->price);
                break;
            }
        }

        ptr += msg_size;
    }
}

