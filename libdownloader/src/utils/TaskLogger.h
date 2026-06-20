#pragma once
#include "Global.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace edm {

class TaskLoggerManager {
public:
    static void init(std::string logDir = "logs");

    static std::shared_ptr<spdlog::logger> getOrCreate(TaskId taskId);
    static void                             remove(TaskId taskId);

    static void setLevel(int level);

    static void shutdown();

private:
    static std::mutex                                         mutex_;
    static std::unordered_map<TaskId, std::shared_ptr<spdlog::logger>> loggers_;
    static std::string                                        logDir_;
    static bool                                               initialized_;
};

} // namespace edm

#define EDM_LOG_TRACE(taskId, ...) \
    do { \
        if (auto logger_ = ::edm::TaskLoggerManager::getOrCreate(taskId); logger_) \
            logger_->trace(__VA_ARGS__); \
    } while (0)

#define EDM_LOG_DEBUG(taskId, ...) \
    do { \
        if (auto logger_ = ::edm::TaskLoggerManager::getOrCreate(taskId); logger_) \
            logger_->debug(__VA_ARGS__); \
    } while (0)

#define EDM_LOG_INFO(taskId, ...) \
    do { \
        if (auto logger_ = ::edm::TaskLoggerManager::getOrCreate(taskId); logger_) \
            logger_->info(__VA_ARGS__); \
    } while (0)

#define EDM_LOG_WARN(taskId, ...) \
    do { \
        if (auto logger_ = ::edm::TaskLoggerManager::getOrCreate(taskId); logger_) \
            logger_->warn(__VA_ARGS__); \
    } while (0)

#define EDM_LOG_ERROR(taskId, ...) \
    do { \
        if (auto logger_ = ::edm::TaskLoggerManager::getOrCreate(taskId); logger_) \
            logger_->error(__VA_ARGS__); \
    } while (0)

#define EDM_LOG_CRITICAL(taskId, ...) \
    do { \
        if (auto logger_ = ::edm::TaskLoggerManager::getOrCreate(taskId); logger_) \
            logger_->critical(__VA_ARGS__); \
    } while (0)
