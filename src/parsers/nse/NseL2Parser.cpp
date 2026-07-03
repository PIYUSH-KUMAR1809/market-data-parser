#include "parsers/nse/NseL2Parser.hpp"
#include "parsers/nse/NseL2Protocol.hpp"
#include "minilzo/minilzo.h"
#include "Endian.hpp"
#include "spdlog/spdlog.h"
#include <string_view>
#include <cstring>

namespace NseL2 {

NseL2Parser::NseL2Parser() {
    if (lzo_init() != LZO_E_OK) {
        spdlog::error("LZO initialization failed");
    }
    decompressBuffer_.resize(64 * 1024);
}

NseL2Parser::~NseL2Parser() {}

static uint8_t hexCharToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

void NseL2Parser::parseJsonLine(const std::string& line) {
    std::string_view searchKey = "\"payload_hex\": \"";
    auto pos = line.find(searchKey);
    if (pos == std::string::npos) return;

    pos += searchKey.length();
    auto endPos = line.find("\"", pos);
    if (endPos == std::string::npos) return;

    std::string_view hexStr = std::string_view(line).substr(pos, endPos - pos);

    std::vector<uint8_t> payload;
    payload.reserve(hexStr.length() / 2);
    for (size_t i = 0; i < hexStr.length(); i += 2) {
        uint8_t byte = (hexCharToInt(hexStr[i]) << 4) | hexCharToInt(hexStr[i+1]);
        payload.push_back(byte);
    }

    parseUdpPayload(payload.data(), payload.size());
}

void NseL2Parser::parseUdpPayload(const uint8_t* payload, size_t size) {
    if (size < sizeof(ST_COMP_BATCH_HEADER)) return;

    const auto* header = reinterpret_cast<const ST_COMP_BATCH_HEADER*>(payload);

    int16_t dataSize = MarketData::big_to_native(header->nDataSize);
    int16_t numPackets = MarketData::big_to_native(header->iNoOfPackets);
    (void)numPackets;


    if (header->cCompOrNot == 0x02 || header->cCompOrNot == 0x00) {
        if (decompressBuffer_.size() < (size_t)dataSize) {
            decompressBuffer_.resize(dataSize);
        }

        lzo_uint outLen = decompressBuffer_.size();
        int r = lzo1x_decompress_safe(payload + sizeof(ST_COMP_BATCH_HEADER),
                                      size - sizeof(ST_COMP_BATCH_HEADER),
                                      decompressBuffer_.data(),
                                      &outLen,
                                      nullptr);
        if (r != LZO_E_OK) {
            spdlog::error("LZO decompression failed with code: {}", r);
            return;
        }

        processDecompressedData(decompressBuffer_.data(), outLen);
    } else {
        processDecompressedData(payload + sizeof(ST_COMP_BATCH_HEADER), size - sizeof(ST_COMP_BATCH_HEADER));
    }
}

void NseL2Parser::processDecompressedData(const uint8_t* data, size_t size) {
    size_t offset = 0;
    while (offset + sizeof(ST_INFO_HEADER) <= size) {
        const auto* infoHdr = reinterpret_cast<const ST_INFO_HEADER*>(data + offset);
        int16_t iLen = MarketData::big_to_native(infoHdr->iLen);

        if (iLen == 0 || offset + iLen > size) {
            break;
        }

        char code1 = infoHdr->iCode[0];
        char code2 = infoHdr->iCode[1];

        if (code1 == 'F' && code2 == 'T') {
            spdlog::info("Parsed BOD Master Information (FT)");
        } else if (code1 == 'F' && code2 == 'N') {
            const auto* fnUpdate = reinterpret_cast<const NormalMarketContract5DepthUpdate*>(
                data + offset + sizeof(ST_INFO_HEADER));
            std::string_view symbol(fnUpdate->symbol, 10);
            spdlog::debug("Parsed Normal Market Update (FN) for symbol: {}", symbol);
        } else if (code1 == 'P' && code2 == 'N') {
            spdlog::debug("Parsed Pre-open Market Update (PN)");
        }

        offset += iLen;
    }
}

}
