#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace edm::script {

/// 脚本控制台日志级别
enum class LogLevel : int { Debug = 0, Info = 1, Warn = 2, Error = 3 };

/// 单个脚本的控制台日志缓冲（环形，满后丢弃最旧行）。
/// 脚本核心层负责写入；展示层通过变更回调刷新（模型/UI）。
class ScriptConsole {
public:
    struct Entry {
        std::string time;
        int         level{0};
        std::string message;
    };

    static constexpr std::size_t kMaxEntries = 1000;

    using ChangeCallback = std::function<void()>;

    void append(LogLevel level, std::string const& message);
    void clear();

    [[nodiscard]] std::deque<Entry> const& entries() const { return entries_; }

    /// 内容变化（追加/清空）时回调；仅主线程设置与触发
    void setChangeCallback(ChangeCallback callback) { onChanged_ = std::move(callback); }

private:
    std::deque<Entry> entries_;
    ChangeCallback    onChanged_;
};

} // namespace edm::script
