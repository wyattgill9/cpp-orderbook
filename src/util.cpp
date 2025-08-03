#include <chrono>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <utility>

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

enum LogLevel { DEBUG, INFO, WARNING, ERROR };
#define LOG(level, msg) \
    if (level >= CURRENT_LOG_LEVEL) \
        std::cout << #level << ": " << __FILE__ << ":" << __LINE__ << " - " << msg << std::endl
#define CURRENT_LOG_LEVEL DEBUG

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

template<typename T, std::size_t N>
class BoundedQueue {
    alignas(T) std::byte buffer[N * sizeof(T)];
    std::size_t head, tail, count;
    
    T* slot(std::size_t index) {
        return reinterpret_cast<T*>(buffer + index * sizeof(T));
    }
    
    const T* slot(std::size_t index) const {
        return reinterpret_cast<const T*>(buffer + index * sizeof(T));
    }
    
public:
    BoundedQueue() : head(0), tail(0), count(0) {}
    ~BoundedQueue() {
        clear();
    }
    
    const T& operator[](std::size_t index) const {
        return *slot((head + index) % N);
    }
    
    T& operator[](std::size_t index) {
        return *slot((head + index) % N);
    }
    
    bool push_back(const T& item) {
        if (count == N) return false;
        new(slot(tail)) T(item);  
        tail = (tail + 1) % N;
        count++;
        return true;
    }
    
    bool push_back(T&& item) {
        if (count == N) return false;
        new(slot(tail)) T(std::move(item));
        tail = (tail + 1) % N;
        count++;
        return true;
    }
    
    template<typename... Args>
    bool emplace_back(Args&&... args) {
        if (count == N) return false;
        new(slot(tail)) T(std::forward<Args>(args)...);
        tail = (tail + 1) % N;
        count++;
        return true;
    }
    
    T pop_front() {
        if (count == 0) throw std::runtime_error("Cannot pop from BoundedQueue with size 0");
        T* ptr = slot(head);
        T item = std::move(*ptr);
        ptr->~T();
        head = (head + 1) % N;
        count--;
        return item;
    }
    
    const T& front() const {
        if (count == 0) throw std::runtime_error("Cannot access front of empty BoundedQueue");
        return *slot(head);
    }
    
    T& front() {
        if (count == 0) throw std::runtime_error("Cannot access front of empty BoundedQueue");
        return *slot(head);
    }
    
    const T& back() const {
        if (count == 0) throw std::runtime_error("Cannot access back of empty BoundedQueue");
        return *slot((tail + N - 1) % N);
    }
    
    T& back() {
        if (count == 0) throw std::runtime_error("Cannot access back of empty BoundedQueue");
        return *slot((tail + N - 1) % N);
    }
    
    void clear() {
        while (count > 0) {
            slot(head)->~T();
            head = (head + 1) % N;
            count--;
        }
    }
    
    bool empty() const { return count == 0; }
    bool full() const { return count == N; }
    std::size_t size() const { return count; }
    constexpr std::size_t capacity() const { return N; }
    
    class Iterator {
        const BoundedQueue* queue;
        std::size_t index;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;
        
        Iterator(const BoundedQueue* q, std::size_t i) : queue(q), index(i) {}
        const T& operator*() const { return (*queue)[index]; }
        const T* operator->() const { return &(*queue)[index]; }
        Iterator& operator++() { ++index; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++index; return tmp; }
        bool operator==(const Iterator& other) const { return index == other.index; }
        bool operator!=(const Iterator& other) const { return index != other.index; }
    };
    
    Iterator begin() const { return Iterator(this, 0); }
    Iterator end() const { return Iterator(this, count); }
    Iterator cbegin() const { return Iterator(this, 0); }
    Iterator cend() const { return Iterator(this, count); }    
};
