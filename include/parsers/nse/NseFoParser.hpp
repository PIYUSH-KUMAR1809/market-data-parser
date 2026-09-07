#ifndef NSE_FO_PARSER_HPP
#define NSE_FO_PARSER_HPP

#include <cstddef>

#include "ShardManager.hpp"

namespace NseFo {

class NseFoParser {
   public:
    NseFoParser() = default;

    MarketData::ShardManager shardManager;

    size_t getTotalOrderCount() const { return shardManager.getTotalOrderCount(); }

    void parseBuffer(const char* buffer, size_t size);
    void parseBufferParallel(const char* buffer, size_t size, size_t numThreads = 0);

   private:
    void parseBlock(const char* block);
};

}

#endif
