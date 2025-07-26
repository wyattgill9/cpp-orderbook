#include <chrono>
#include <type_traits>

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
