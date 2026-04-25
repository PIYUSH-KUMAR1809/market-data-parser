#ifndef NSE_FO_PROTOCOL_HPP
#define NSE_FO_PROTOCOL_HPP

#include <cstdint>

namespace NseFo {

#pragma pack(push, 1)

struct MessageHeader {
    int16_t transCode;
    int32_t logTime;
    int32_t alphaChar;
    int16_t transCode2;
    int16_t errorCode;
    int32_t timestamp;
    char timestamp2[8];
    int16_t messageLen;
};

struct SnapshotRecord {
    int32_t token;
    int32_t price;
    int32_t quantity;
    int32_t unknown1;
    int32_t unknown2;
};

constexpr int32_t MAX_TOKEN_ID = 200000;
constexpr int32_t MAX_PRICE_LIMIT = 100000000;
constexpr int32_t MAX_QTY_LIMIT = 10000000;

#pragma pack(pop)

}  // namespace NseFo

#endif
