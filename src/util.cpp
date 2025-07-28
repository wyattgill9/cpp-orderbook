#include <chrono>
#include <cctype>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

template <typename Enum>
constexpr auto to_underlying(Enum e) {
    return static_cast<std::underlying_type_t<Enum>>(e);
}

template<typename Func>
u64 time_ns(Func&& func) {
    auto start = std::chrono::steady_clock::now();
    func();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}
