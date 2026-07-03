#pragma once

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <memory>
#include <string>

namespace MarketData {

class Logger {
   public:
    static void init(bool console = true, const std::string& logFile = "market_data.log") {
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
            file_sink->set_level(spdlog::level::trace);

            std::vector<spdlog::sink_ptr> sinks;
            if (console) sinks.push_back(console_sink);
            sinks.push_back(file_sink);

            spdlog::init_thread_pool(8192, 1);
            auto logger =
                std::make_shared<spdlog::async_logger>("multi_sink",
                                                       sinks.begin(),
                                                       sinks.end(),
                                                       spdlog::thread_pool(),
                                                       spdlog::async_overflow_policy::block);

            spdlog::set_default_logger(logger);
            spdlog::set_pattern("[%C-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
            spdlog::flush_on(spdlog::level::warn);

            spdlog::info("Logger initialized");
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << "\n";
        }
    }
};

}
