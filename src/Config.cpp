#include "Config.hpp"

#include <toml++/toml.h>

#include <stdexcept>

namespace MarketData {

Config Config::load(const std::string& configPath) {
    Config config;

    try {
        toml::table tbl = toml::parse_file(configPath);

        if (tbl["file"].is_string()) {
            config.file = tbl["file"].value<std::string>().value();
        }

        if (tbl["mode"].is_string()) {
            config.mode = tbl["mode"].value<std::string>().value();
        }

        if (tbl["logging"].is_table()) {
            auto logging = tbl["logging"];
            config.logging.level = logging["level"].value_or("info");
            config.logging.file = logging["file"].value_or("market_data.log");
            config.logging.console = logging["console"].value_or(true);
        }

    } catch (const toml::parse_error& err) {
        throw std::runtime_error("Failed to parse config file: " + std::string(err.description()));
    }

    return config;
}

}  // namespace MarketData
