#include "utils/TaskLogger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>
#include <vector>

namespace edm {

std::mutex                                                    TaskLoggerManager::mutex_;
std::unordered_map<TaskId, std::shared_ptr<spdlog::logger>>   TaskLoggerManager::loggers_;
std::string                                                   TaskLoggerManager::logDir_{"logs"};
bool                                                          TaskLoggerManager::initialized_{false};

void TaskLoggerManager::init(std::string logDir) {
    std::lock_guard lock(mutex_);
    if (initialized_) return;

    logDir_ = std::move(logDir);
    std::error_code ec;
    std::filesystem::create_directories(logDir_, ec);

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
    spdlog::set_level(spdlog::level::debug);

    initialized_ = true;
}

std::shared_ptr<spdlog::logger> TaskLoggerManager::getOrCreate(TaskId taskId) {
    std::lock_guard lock(mutex_);
    if (!initialized_) {
        logDir_ = "logs";
        std::error_code ec;
        std::filesystem::create_directories(logDir_, ec);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
        spdlog::set_level(spdlog::level::debug);
        initialized_ = true;
    }

    auto it = loggers_.find(taskId);
    if (it != loggers_.end()) {
        return it->second;
    }

    auto filename = std::filesystem::path(logDir_) / ("task_" + std::to_string(taskId) + ".log");

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename.string(), true));
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    auto logger = std::make_shared<spdlog::logger>("task_" + std::to_string(taskId), sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::warn);

    spdlog::register_logger(logger);
    loggers_[taskId] = logger;
    return logger;
}

void TaskLoggerManager::remove(TaskId taskId) {
    std::lock_guard lock(mutex_);
    auto it = loggers_.find(taskId);
    if (it != loggers_.end()) {
        it->second->flush();
        spdlog::drop(it->second->name());
        loggers_.erase(it);
    }
}

void TaskLoggerManager::setLevel(int level) {
    std::lock_guard lock(mutex_);
    auto lvl = static_cast<spdlog::level::level_enum>(level);
    spdlog::set_level(lvl);
    for (auto& [id, logger] : loggers_) {
        logger->set_level(lvl);
    }
}

void TaskLoggerManager::shutdown() {
    std::lock_guard lock(mutex_);
    for (auto& [id, logger] : loggers_) {
        logger->flush();
    }
    loggers_.clear();
    spdlog::shutdown();
    initialized_ = false;
}

} // namespace edm
