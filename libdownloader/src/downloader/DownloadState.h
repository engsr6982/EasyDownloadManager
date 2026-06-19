#pragma once
#include "Global.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace edm::downloader {

struct DownloadRange {
    int64_t              start{0};
    int64_t              end{0};
    std::atomic<int64_t> downloaded{0};
    std::atomic<int>     attempts{0};
    std::atomic<bool>    completed{false};

    DownloadRange(int64_t s, int64_t e) : start(s), end(e) {}

    [[nodiscard]] int64_t size() const noexcept {
        return end >= start ? end - start + 1 : 0;
    }
};

class DownloadState {
    std::atomic<int>     state_{static_cast<int>(TaskState::Pending)};
    std::atomic<int64_t> downloadedBytes_{0};
    std::atomic<int64_t> totalBytes_{kInvalidFileSize};
    std::atomic<double>  speedBytesPerSecond_{0.0};

    mutable std::shared_mutex stringsMutex_;
    std::string               errorMessage_;
    std::string               outputPath_;

public:
    std::vector<std::shared_ptr<DownloadRange>> ranges;

    void setState(TaskState state) noexcept { state_.store(static_cast<int>(state), std::memory_order_release); }
    [[nodiscard]] TaskState state() const noexcept {
        return static_cast<TaskState>(state_.load(std::memory_order_acquire));
    }

    void setDownloadedBytes(int64_t bytes) noexcept { downloadedBytes_.store(bytes, std::memory_order_relaxed); }
    [[nodiscard]] int64_t downloadedBytes() const noexcept {
        return downloadedBytes_.load(std::memory_order_relaxed);
    }

    void setTotalBytes(int64_t bytes) noexcept { totalBytes_.store(bytes, std::memory_order_relaxed); }
    [[nodiscard]] int64_t totalBytes() const noexcept { return totalBytes_.load(std::memory_order_relaxed); }

    void setSpeed(double bytesPerSecond) noexcept {
        speedBytesPerSecond_.store(bytesPerSecond, std::memory_order_relaxed);
    }
    [[nodiscard]] double speed() const noexcept { return speedBytesPerSecond_.load(std::memory_order_relaxed); }

    void setErrorMessage(std::string message) {
        std::unique_lock lock(stringsMutex_);
        errorMessage_ = std::move(message);
    }
    [[nodiscard]] std::string errorMessage() const {
        std::shared_lock lock(stringsMutex_);
        return errorMessage_;
    }

    void setOutputPath(std::string path) {
        std::unique_lock lock(stringsMutex_);
        outputPath_ = std::move(path);
    }
    [[nodiscard]] std::string outputPath() const {
        std::shared_lock lock(stringsMutex_);
        return outputPath_;
    }
};

} // namespace edm::downloader
