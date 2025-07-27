#include <cstdint>

struct MessageA {
    uint16_t value1;
    float    value2;
};

struct MessageB {
    uint32_t timestamp;
    uint8_t  level;
    uint8_t  flags;
};

enum MessageType : uint8_t {
    MSG_A = 0x01,
    MSG_B = 0x02
};

struct ParsedMessage {
    MessageType type;
    const void* payload;
};

if (msg.type == MSG_A) {
    auto* a = reinterpret_cast<const MessageA*>(msg.payload);
    std::cout << a->value1 << ", " << a->value2 << "\n";
}
