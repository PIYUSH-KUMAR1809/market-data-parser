#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>

#include "Config.hpp"
#include "ItchParser.hpp"
#include "Logger.hpp"
#include "parsers/nse/NseFoParser.hpp"
#include "parsers/nse/NseL2Parser.hpp"
#include <fstream>
#include <iostream>

struct MappedFile {
    char *data = nullptr;
    size_t size = 0;
    int fd = -1;

    MappedFile(const std::string &path) {
        fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            spdlog::error("Error opening file: {}", path);
            return;
        }

        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            close(fd);
            return;
        }
        size = sb.st_size;

        data = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (data == MAP_FAILED) {
            spdlog::error("mmap failed");
            data = nullptr;
            size = 0;
            close(fd);
            fd = -1;
        }
    }

    ~MappedFile() {
        if ((data != nullptr) && data != MAP_FAILED) {
            munmap(data, size);
        }
        if (fd != -1) {
            close(fd);
        }
    }
};

int main(int argc, char **argv) {
    MarketData::Logger::init();

    if (argc < 2) {
        spdlog::error("Usage: {} <file> [mode: itch|nse]", argv[0]);
        return 1;
    }

    std::string filePath;
    std::string mode = "itch";

    std::string arg1 = argv[1];
    if (arg1.ends_with(".toml")) {
        try {
            auto config = MarketData::Config::load(arg1);
            filePath = config.file;
            mode = config.mode;

            MarketData::Logger::init(config.logging.console, config.logging.file);
            spdlog::info("Loaded config from {}", arg1);
        } catch (const std::exception &e) {
            spdlog::error("Error loading config: {}", e.what());
            return 1;
        }
    } else {
        filePath = arg1;
        if (argc >= 3) {
            mode = argv[2];
        } else {
            if (filePath.find(".bin") != std::string::npos ||
                filePath.find("NSE") != std::string::npos) {
                mode = "nse";
            }
        }
    }

    spdlog::info("Processing file: {} in mode: {}", filePath, mode);
    
    if (mode == "nse_l2_jsonl") {
        NseL2::NseL2Parser parser;
        std::ifstream file(filePath);
        if (!file.is_open()) {
            spdlog::error("Failed to open jsonl file: {}", filePath);
            return 1;
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        std::string line;
        int count = 0;
        while (std::getline(file, line)) {
            parser.parseJsonLine(line);
            count++;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        
        spdlog::info("Finished parsing {} lines of NSE L2 JSONL in {} seconds.", count, diff.count());
        return 0;
    }

    MappedFile file(filePath);
    if (file.data == nullptr) return 1;
    spdlog::info("File size: {} MB", file.size / (static_cast<size_t>(1024 * 1024)));

    if (mode == "nse") {
        NseFo::NseFoParser parser;

        auto start = std::chrono::high_resolution_clock::now();
        parser.parseBuffer(file.data, file.size);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        double seconds = diff.count();

        spdlog::info("Finished NSE processing in {} seconds.", seconds);
        spdlog::info("Active Orders in Book: {}", parser.getTotalOrderCount());

    } else {
        MarketData::ItchParser parser;
        parser.printDebug = false;

        auto start = std::chrono::high_resolution_clock::now();
        parser.parseBuffer(file.data, file.size);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        double seconds = diff.count();
        double messagesPerSecond = static_cast<double>(parser.messagesParsed) / seconds / 1e6;

        spdlog::info("Parsed {} messages in {} seconds.", parser.messagesParsed, seconds);
        spdlog::info("Speed: {} Million messages/sec", messagesPerSecond);
        spdlog::info("Active Orders in Book: {}", parser.getTotalOrderCount());
        parser.printStats();

        if (parser.verifyComplete(file.size)) {
            spdlog::info("SUCCESS: Parser consumed exactly {} bytes.", file.size);
        } else {
            spdlog::error("FAILURE: Parser byte mismatch.");
        }

        if (parser.unhandledMessages > 0) {
            spdlog::warn("WARNING: Encountered {} unhandled message types.",
                         parser.unhandledMessages);
        } else {
            spdlog::info("SUCCESS: No unhandled/unknown message types encountered.");
        }
    }

    return 0;
}
