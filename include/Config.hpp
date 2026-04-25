#pragma once

#include <string>

namespace MarketData {

struct Config {

    struct Logging {
        std::string level = "info";
        std::string file = "market_data.log";
        bool console = true;
    };

    std::string file;
    std::string mode;
    Logging logging;

    static Config load(const std::string& configPath);
};

}  
