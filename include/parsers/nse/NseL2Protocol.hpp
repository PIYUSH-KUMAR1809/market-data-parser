#pragma once

#include <cstdint>

namespace NseL2 {

#pragma pack(push, 1)


struct ST_COMP_BATCH_HEADER {
    char cCompOrNot;
    int16_t nDataSize;
    int16_t iNoOfPackets;
};


struct ST_INFO_HEADER {
    char iCode[2];
    int16_t iLen;
    int32_t lSeqNo;
};


struct ST_INFO_TRAILER {
    int16_t iCheckSum;
    char cEOT;
};

struct BOD_Master_Information {
    char token[10];
    char instrumentType[6];
    char symbol[10];
    char expiryDate[11];
    char strikePrice[10];
    char optionType[2];
    char category;
    char deleteFlag;
    char lowPriceRange[10];
    char highPriceRange[10];

    struct ContractEligibility {
        char marketType;
        char eligibility;
        char contractStatus;
    } eligibility[4];

    char contractName[25];
    char regularLot[10];
    char tickSize[10];
    char maturityDate[10];
};

struct DepthOrderInfo {
    char price[10];
    char quantity[12];
};

struct NormalMarketContract5DepthUpdate {
    char instrumentType[6];
    char symbol[10];
    char expiryDate[11];
    char strikePrice[10];
    char optionType[2];
    char marketType;
    char timestamp[11];

    DepthOrderInfo buy[5];
    DepthOrderInfo sell[5];

    char lastTradedPrice[10];
    char totalTradedQuantity[12];
    char securityStatus;
    char openingPrice[10];
    char highPrice[10];
    char lowPrice[10];
    char closePrice[10];
    char averageTradePrice[10];
    char totalBuyQuantity[12];
    char totalSellQuantity[12];
    char totalTurnover[25];
};

#pragma pack(pop)

}
