#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include "Config.hpp"
#include "ItchParser.hpp"
#include "Logger.hpp"
#include "parsers/nse/NseFoParser.hpp"
#include "parsers/nse/NseFoProtocol.hpp"
#ifdef MARKET_DATA_HAS_NSEL2_LZO
#include "parsers/nse/NseL2Parser.hpp"
#endif

struct MappedFile {
    char*  data = nullptr;
    size_t size = 0;
    int    fd   = -1;

    MappedFile(const std::string& path) {
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

        data = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (data == MAP_FAILED) {
            spdlog::error("mmap failed");
            data = nullptr;
            size = 0;
            close(fd);
            fd = -1;
            return;
        }

#if defined(MADV_SEQUENTIAL)
        madvise(data, size, MADV_SEQUENTIAL);
#endif
#if defined(MADV_WILLNEED)
        madvise(data, size, MADV_WILLNEED);
#endif
    }

    ~MappedFile() {
        if ((data != nullptr) && data != MAP_FAILED) munmap(data, size);
        if (fd != -1) close(fd);
    }
};

static void parseNseDoubleBuf(int fd, size_t fileSize, NseFo::NseFoParser& parser) {
    constexpr size_t CHUNK_SIZE = 256ULL * 1024 * 1024;
    constexpr size_t RECORD_SIZE = sizeof(NseFo::SnapshotRecord);

    auto buf0 = std::make_unique<char[]>(CHUNK_SIZE);
    auto buf1 = std::make_unique<char[]>(CHUNK_SIZE);
    char* bufs[2] = {buf0.get(), buf1.get()};

    size_t offset = 0;
    int   curBuf  = 0;

    size_t toRead0 = std::min(CHUNK_SIZE, fileSize);
    toRead0 = (toRead0 / RECORD_SIZE) * RECORD_SIZE;
    ssize_t got0 = pread(fd, bufs[0], toRead0, 0);
    if (got0 <= 0) return;
    size_t filled0 = static_cast<size_t>(got0);
    offset += filled0;

    while (filled0 > 0) {
        int     nextBuf    = 1 - curBuf;
        size_t  nextOffset = offset;
        size_t  nextFill   = 0;

        std::thread ioThread;
        if (nextOffset < fileSize) {
            ioThread = std::thread([&, nextBuf, nextOffset]() {
                size_t toRead = std::min(CHUNK_SIZE, fileSize - nextOffset);
                toRead = (toRead / RECORD_SIZE) * RECORD_SIZE;
                ssize_t n     = pread(fd, bufs[nextBuf], toRead, static_cast<off_t>(nextOffset));
                nextFill      = (n > 0) ? static_cast<size_t>(n) : 0;
            });
        }

        parser.parseBuffer(bufs[curBuf], filled0);

        if (ioThread.joinable()) ioThread.join();

        offset  += nextFill;
        filled0  = nextFill;
        curBuf   = nextBuf;
    }
}

int main(int argc, char** argv) {
    MarketData::Logger::init();

    if (argc < 2) {
        spdlog::error("Usage: {} <file> [mode: itch|nse|nse_l2_jsonl]", argv[0]);
        return 1;
    }

    std::string filePath;
    std::string mode = "itch";

    std::string arg1 = argv[1];
    struct stat argStat;
    if (stat(arg1.c_str(), &argStat) != 0 && !arg1.starts_with("data/")) {
        std::string candidate = "data/" + arg1;
        if (stat(candidate.c_str(), &argStat) == 0) {
            arg1 = candidate;
        }
    }
    if (arg1.ends_with(".toml")) {
        try {
            auto config = MarketData::Config::load(arg1);
            filePath    = config.file;
            mode        = config.mode;

            MarketData::Logger::init(config.logging.console, config.logging.file);
            spdlog::info("Loaded config from {}", arg1);
        } catch (const std::exception& e) {
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
#ifndef MARKET_DATA_HAS_NSEL2_LZO
        spdlog::error(
            "NSE L2 support is disabled. Rebuild with -DENABLE_NSE_L2_LZO=ON "
            "(links miniLZO; the binary becomes a GPL-2+ combined work).");
        return 1;
#else
        NseL2::NseL2Parser parser;
        std::ifstream      file(filePath);
        if (!file.is_open()) {
            spdlog::error("Failed to open jsonl file: {}", filePath);
            return 1;
        }

        auto        start = std::chrono::high_resolution_clock::now();
        std::string line;
        int         count = 0;
        while (std::getline(file, line)) {
            parser.parseJsonLine(line);
            count++;
        }
        auto                          end  = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;

        spdlog::info("Finished parsing {} lines of NSE L2 JSONL in {} seconds.", count, diff.count());
        return 0;
#endif
    }

    if (mode == "nse") {
        int fd = open(filePath.c_str(), O_RDONLY);
        if (fd == -1) {
            spdlog::error("Error opening file: {}", filePath);
            return 1;
        }

        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            spdlog::error("fstat failed");
            close(fd);
            return 1;
        }
        size_t fileSize = static_cast<size_t>(sb.st_size);
        spdlog::info("File size: {} MB", fileSize / (1024ULL * 1024));

        NseFo::NseFoParser parser;

        auto start = std::chrono::high_resolution_clock::now();
        parseNseDoubleBuf(fd, fileSize, parser);
        auto end = std::chrono::high_resolution_clock::now();
        close(fd);

        std::chrono::duration<double> diff    = end - start;
        double                        seconds = diff.count();

        spdlog::info("Finished NSE processing in {} seconds.", seconds);
        spdlog::info("Active Orders in Book: {}", parser.getTotalOrderCount());

    } else {
        MappedFile file(filePath);
        if (file.data == nullptr) return 1;
        spdlog::info("File size: {} MB", file.size / (static_cast<size_t>(1024 * 1024)));

        MarketData::ItchParser parser;
        parser.printDebug = false;

        auto start = std::chrono::high_resolution_clock::now();
        parser.parseBuffer(file.data, file.size);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff             = end - start;
        double                        seconds          = diff.count();
        double                        messagesPerSecond = static_cast<double>(parser.messagesParsed) / seconds / 1e6;

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
