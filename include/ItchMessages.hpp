#pragma once

#include <cstdint>

#include "Endian.hpp"

namespace MarketData {

#pragma pack(push, 1)

struct MessageHeader {
    char messageType;
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp48 : 48;
};

struct ItchHeader {
    char messageType;
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint8_t timestamp[6];

    uint64_t getTimestamp() const {
        uint64_t high = big_to_native<uint16_t>(*(uint16_t *)timestamp);
        uint32_t low = big_to_native<uint32_t>(*(uint32_t *)(timestamp + 2));
        return (high << 32) | low;
    }
};

struct SystemEventMsg {
    ItchHeader header;
    char eventCode;
};

struct AddOrderMsg {
    ItchHeader header;
    uint64_t orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
};

struct OrderExecutedMsg {
    ItchHeader header;
    uint64_t orderReferenceNumber;
    uint32_t executedShares;
    uint64_t matchNumber;
};

struct TradeMsg {
    ItchHeader header;
    uint64_t orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
    uint64_t matchNumber;
};

struct AddOrderMPIDMsg {
    ItchHeader header;
    uint64_t orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
    char attribution[4];
};

struct NOIIMsg {
    ItchHeader header;
    uint64_t pairedShares;
    uint64_t imbalanceShares;
    char imbalanceDirection;
    char stock[8];
    uint32_t farPrice;
    uint32_t nearPrice;
    uint32_t currentReferencePrice;
    char crossType;
    char priceVariationIndicator;
};

struct OrderExecutedWithPriceMsg {
    ItchHeader header;
    uint64_t orderReferenceNumber;
    uint32_t executedShares;
    uint64_t matchNumber;
    char printable;
    uint32_t executionPrice;
};

struct OrderCancelMsg {
    ItchHeader header;
    uint64_t orderReferenceNumber;
    uint32_t canceledShares;
};

struct OrderDeleteMsg {
    ItchHeader header;
    uint64_t orderReferenceNumber;
};

struct OrderReplaceMsg {
    ItchHeader header;
    uint64_t originalOrderReferenceNumber;
    uint64_t newOrderReferenceNumber;
    uint32_t shares;
    uint32_t price;
};

struct StockDirectoryMsg {
    ItchHeader header;
    char stock[8];
    char marketCategory;
    char financialStatusIndicator;
    uint32_t roundLotSize;
    char roundLotsOnly;
    char issueClassification;
    char issueSubType[2];
    char authenticity;
    char shortSaleThresholdIndicator;
    char ipoFlag;
    char luldReferencePriceTier;
    char etpFlag;
    uint32_t etpLeverageFactor;
    char inverseIndicator;
};

#pragma pack(pop)

}  // namespace MarketData
