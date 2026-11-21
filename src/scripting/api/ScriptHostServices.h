#pragma once

#include "Global.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace edm::script {

/// 托盘通知级别
enum class NotifyLevel : int { Info = 0, Warning = 1, Error = 2 };

/// 任务控制动作
enum class TaskAction : int { Pause = 0, Resume = 1, Cancel = 2 };

/// 通知选项
struct NotifyOptions {
    std::optional<std::string> title;
    std::optional<NotifyLevel> level;
};

/// 脚本侧任务快照（宿主从数据库/引擎组装，绑定层转换为 TaskInfo）
struct TaskSnapshot {
    int          id{0};
    std::string  url;
    std::string  fileName;
    std::string  saveDir;
    TaskState    state{TaskState::Pending};
    std::int64_t fileSize{-1};
    double       downloaded{0};
    double       speed{0};
    int          threadCount{0};
};

/// 脚本运行时的宿主服务接口。
/// 脚本核心层不依赖 Qt：定时器、事件泵、托盘通知、配置读写、任务查询均由宿主实现；
/// 除特别说明外，方法在主线程（脚本线程）调用。
class ScriptHostServices {
public:
    virtual ~ScriptHostServices() = default;

    // ---- 环境 ----
    [[nodiscard]] virtual std::string scriptsDir() const    = 0;
    [[nodiscard]] virtual std::string appDataDir() const    = 0;
    [[nodiscard]] virtual std::string userAgent() const     = 0;
    [[nodiscard]] virtual int         hookTimeoutMs() const = 0;

    // ---- 脚本启用状态持久化 ----
    [[nodiscard]] virtual std::vector<std::string> disabledScripts() const                                   = 0;
    virtual void                                   setDisabledScripts(std::vector<std::string> const& names) = 0;

    // ---- 任务查询/控制（供 tasks 绑定）----
    [[nodiscard]] virtual std::optional<TaskSnapshot> taskGet(int id) const                  = 0;
    [[nodiscard]] virtual std::vector<TaskSnapshot>   taskGetAll() const                     = 0;
    virtual bool                                      taskControl(int id, TaskAction action) = 0;

    // ---- 定时器：调度回调并返回句柄（回调在主线程执行）----
    virtual std::int64_t startTimer(std::function<void()> const& fn, int ms, bool singleShot) = 0;
    virtual void         stopTimer(std::int64_t handle)                                       = 0;

    // ---- 低频泵：宿主周期性在主线程驱动回调（推进引擎微任务/任务队列）----
    virtual void setPumpHandler(std::function<void()> const& fn, int intervalMs) = 0;

    // ---- 托盘通知 ----
    virtual void notify(NotifyLevel level, std::string const& title, std::string const& message) = 0;

    // ---- 宿主日志（加载失败诊断等）----
    virtual void logWarning(std::string const& message) = 0;
};

} // namespace edm::script
