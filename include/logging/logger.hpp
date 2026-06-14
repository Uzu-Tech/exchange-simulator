#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "primitives.hpp"
#include "settings.hpp"
#include "log_types.hpp"
#include "npy.hpp"

namespace fs = std::filesystem;

static std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t legacy_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = *std::localtime(&legacy_time);
    std::stringstream ss;
    ss << std::put_time(&local_time, "%Y-%m-%d_%H-%M-%S");
    return ss.str();
}

class ScopedFileDesc {
public:
    ScopedFileDesc(const fs::path& path) {
        value_ = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (value_ == -1) {
            throw std::runtime_error(std::format("Failed to open file: {}", strerror(errno)));
        }
    }

    ~ScopedFileDesc() {
        if (value_ != -1) close(value_);
    }

    ScopedFileDesc(const ScopedFileDesc&) = delete;
    ScopedFileDesc& operator=(const ScopedFileDesc&) = delete;

    int value() const noexcept { return value_; }

private:
    int value_ = -1;
};

template<typename Log>
class MappedDatasetFile {
public:
    MappedDatasetFile(const fs::path& file_path, size_t max_logs)
        : file_desc(file_path), max_logs(max_logs) {
        if (max_logs > settings::MAX_NUM_LOGS) {
            throw std::runtime_error("Number of logs written exceed 1 billion in file " + file_path.filename().string());
        }

        header_byte_size = NpyHeader::header_byte_size<Log>();
        size_t num_bytes = sizeof(Log) * max_logs + header_byte_size;
        if (ftruncate(file_desc.value(), num_bytes) == -1) {
            throw std::runtime_error(std::format("Failed to size file: {}", strerror(errno)));
        }

        void* ptr =
            mmap(nullptr, num_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file_desc.value(), 0);

        if (ptr == MAP_FAILED) {
            throw std::runtime_error(std::format("Failed to memory map file: {}", strerror(errno)));
        }

        auto* base = static_cast<std::byte*>(ptr);
        data = std::span<Log>(reinterpret_cast<Log*>(base += header_byte_size), max_logs);
        madvise(ptr, num_bytes, MADV_SEQUENTIAL);

        mapped_ptr = ptr;
        mapped_byte_size = num_bytes;
    }

    void write(Log log) {
        hard_assert(num_logs < max_logs, "File", "Tried to write more logs than allocated space");
        data[num_logs++] = log;
    }

    void write_idx(Log log, size_t idx) {
        hard_assert(idx < max_logs, "File", "Tried to write more logs than allocated space");
        data[idx] = log;
        num_logs++;
    }

    ~MappedDatasetFile() {
        if (data.empty()) return;
        NpyHeader::write_header<Log>(file_desc.value(), num_logs, header_byte_size);

        msync(mapped_ptr, mapped_byte_size, MS_ASYNC);  // Flush data to disk
        munmap(mapped_ptr, mapped_byte_size);

        size_t actual_bytes = num_logs * sizeof(Log) + header_byte_size;
        if (ftruncate(file_desc.value(), actual_bytes) == -1) {
            std::fprintf(
                stderr,
                "Error: Failed to truncate file on destruction: %s\n",
                std::strerror(errno)
            );
        }

        data = std::span<Log>();
    }

private:
    std::span<Log> data;
    ScopedFileDesc file_desc;  // file descriptor
    std::atomic<size_t> num_logs = 0;
    size_t max_logs = 0;
    void*  mapped_ptr = nullptr;
    size_t mapped_byte_size = 0;
    size_t header_byte_size = 0;
};

struct RunDirectory {
    fs::path path;
    RunDirectory(const fs::path& log_dir, const std::string& config_contents, const fs::path& config_path, std::string_view type) {
        fs::create_directories(log_dir / type);
        // Create run directory
        path = log_dir / type / get_timestamp();
        fs::create_directory(path);
        // Create symlink
        fs::path link = log_dir / type / "latest";
        std::error_code ec;
        fs::remove(link, ec); // important: remove old symlink safely
        fs::create_symlink(path.filename(), link);
        // Save config file
        std::ofstream config_file(path / config_path.filename());
        config_file << config_contents;
    }
};

class SingleRunLogger {
public:
    SingleRunLogger(
        const fs::path& log_dir, const std::string& config_contents, const fs::path& config_path, size_t num_ticks
    )
        : run_dir(log_dir, config_contents, config_path, "single"),
          performance_file(run_dir.path / "performance.npy", num_ticks),
          quote_file(run_dir.path / "quotes.npy", num_ticks * settings::MAX_ORDERS_PER_LEVEL * 2),
          trades_file(run_dir.path / "trades.npy", num_ticks * settings::MAX_TRADES_PER_SIDE * 2) {}

    void log_performance_data(PerformanceLog log) { performance_file.write(log); }
    void log_quote_data(QuoteLog log) { quote_file.write(log); }
    void log_trade_data(TradeLog log) { trades_file.write(log); }
    std::string get_run_dir() const { return run_dir.path.string(); }

private:
    RunDirectory run_dir;
    MappedDatasetFile<PerformanceLog> performance_file;
    MappedDatasetFile<QuoteLog> quote_file;
    MappedDatasetFile<TradeLog> trades_file;
};

class MonteCarloLogger {
public:
    MonteCarloLogger(
        const fs::path& log_dir, const std::string& config_contents, const fs::path& config_path, size_t num_runs
    )
        : run_dir(log_dir, config_contents, config_path, "monte-carlo"),
          mc_file(run_dir.path / "monte_carlo_results.npy", num_runs) {}

    void log_monte_carlo_data(MonteCarloLog log, size_t idx) { mc_file.write_idx(log, idx); }
    std::string get_run_dir() const { return run_dir.path.string(); }


private:
    RunDirectory run_dir;
    MappedDatasetFile<MonteCarloLog> mc_file;
};