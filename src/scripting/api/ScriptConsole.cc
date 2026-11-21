#include "ScriptConsole.h"

#include <chrono>
#include <ctime>
#include <fmt/format.h>

namespace edm::script {

namespace {

std::string timestamp() {
    auto const        now = std::chrono::system_clock::now();
    auto const        ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t const t   = std::chrono::system_clock::to_time_t(now);
    std::tm           tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return fmt::format("{:02d}:{:02d}:{:02d}.{:03d}", tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count());
}

} // namespace

void ScriptConsole::append(LogLevel level, std::string const& message) {
    bool const trimmed = entries_.size() >= kMaxEntries;
    if (trimmed) entries_.pop_front();
    entries_.push_back(Entry{timestamp(), static_cast<int>(level), message});
    if (onChanged_) onChanged_();
}

void ScriptConsole::clear() {
    if (entries_.empty()) return;
    entries_.clear();
    if (onChanged_) onChanged_();
}

} // namespace edm::script
