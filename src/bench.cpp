#include "orderbook.hpp"
#include <random>

const void benchmark_orderbook() {
    auto ob = OrderBook("TSLA");
    
    // pseudo-random-number gen
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> price_dist(0.01, 1000.0);
    std::uniform_int_distribution<int> quantity_dist(1, 10000);
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    const int num_orders = 100'000;
    
    for(int i = 0; i < num_orders; i++) {
        double price = price_dist(gen);
        int quantity = quantity_dist(gen);
        char side = side_dist(gen) ? 'B' : 'S';
        
        ob.add_order(price, quantity, side);
    }
}

int main() {
    benchmark_orderbook();
    return 0;
}
