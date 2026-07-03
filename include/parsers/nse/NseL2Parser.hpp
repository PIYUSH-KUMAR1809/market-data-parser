#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace NseL2 {

class NseL2Parser {
public:
    NseL2Parser();
    ~NseL2Parser();


    void parseJsonLine(const std::string& line);


    void parseUdpPayload(const uint8_t* payload, size_t size);

private:
    void processDecompressedData(const uint8_t* data, size_t size);

    std::vector<uint8_t> decompressBuffer_;
};

}
