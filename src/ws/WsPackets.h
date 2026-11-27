#pragma once

#include "WsPacket.h"
#include "scripting/api/ScriptHostServices.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// 协议中的任务状态按枚举名序列化
namespace glz {

template <>
struct meta<edm::TaskState> {
    static constexpr auto value = enumerate(
        "Pending",
        edm::TaskState::Pending,
        "Running",
        edm::TaskState::Running,
        "Paused",
        edm::TaskState::Paused,
        "Canceled",
        edm::TaskState::Canceled,
        "Finished",
        edm::TaskState::Finished,
        "Failed",
        edm::TaskState::Failed,
        "Preparing",
        edm::TaskState::Preparing
    );
};

} // namespace glz

namespace edm::ws {

/// 无参数请求的占位 payload
struct EmptyPayload {};

/// 与脚本宿主共用的任务快照载荷
using TaskSnapshotPayload = script::TaskSnapshot;

// ---- payload ----

struct AuthPayload {
    std::string token;
};

struct CreateTaskPayload {
    std::string                url;
    std::optional<std::string> saveDir;
    std::optional<bool>        useProxy;
    // 浏览器集成的可选接力字段；缺省时按纯 URL 建任务。
    std::optional<std::string>                        referer;
    std::optional<std::string>                        origin;
    std::optional<std::string>                        cookie;
    std::optional<std::string>                        fileName;
    std::optional<std::string>                        mimeType;
    std::optional<std::string>                        pageUrl;
    std::optional<std::string>                        pageTitle;
    std::optional<std::int64_t>                       size;
    std::optional<std::map<std::string, std::string>> headers;
};

struct TaskIdPayload {
    int taskId{0};
};

struct DeleteTaskPayload {
    int  taskId{0};
    bool deleteFiles{false};
};

struct TaskErrorPayload {
    int         taskId{0};
    std::string message;
};

struct TaskFinishedPayload {
    int         taskId{0};
    TaskState   state{TaskState::Pending};
    std::string fileName;
    std::string errorMsg;
};

// ---- packet class ----

struct AuthPacket : WithPacketPayload<AuthPayload> {
    static constexpr std::string_view kMethod = "auth";
    struct Result {
        bool authenticated{false};
    };
};

struct PingPacket : WithPacketPayload<EmptyPayload> {
    static constexpr std::string_view kMethod = "ping";
    struct Result {
        bool pong{true};
    };
};

struct CreateTaskPacket : WithPacketPayload<CreateTaskPayload> {
    static constexpr std::string_view kMethod = "createTask";
    struct Result {
        int taskId{0};
    };
};

struct ListTasksPacket : WithPacketPayload<EmptyPayload> {
    static constexpr std::string_view kMethod = "listTasks";
    struct Result {
        std::vector<TaskSnapshotPayload> tasks;
    };
};

struct GetTaskPacket : WithPacketPayload<TaskIdPayload> {
    static constexpr std::string_view kMethod = "getTask";
    struct Result {
        TaskSnapshotPayload task;
    };
};

struct PauseTaskPacket : WithPacketPayload<TaskIdPayload> {
    static constexpr std::string_view kMethod = "pauseTask";
    struct Result {
        int taskId{0};
    };
};

struct ResumeTaskPacket : WithPacketPayload<TaskIdPayload> {
    static constexpr std::string_view kMethod = "resumeTask";
    struct Result {
        int taskId{0};
    };
};

struct CancelTaskPacket : WithPacketPayload<TaskIdPayload> {
    static constexpr std::string_view kMethod = "cancelTask";
    struct Result {
        int taskId{0};
    };
};

struct DeleteTaskPacket : WithPacketPayload<DeleteTaskPayload> {
    static constexpr std::string_view kMethod = "deleteTask";
    struct Result {
        int taskId{0};
    };
};

struct RedownloadTaskPacket : WithPacketPayload<TaskIdPayload> {
    static constexpr std::string_view kMethod = "redownloadTask";
    struct Result {
        int taskId{0};
    };
};

struct PauseAllPacket : WithPacketPayload<EmptyPayload> {
    static constexpr std::string_view kMethod = "pauseAll";
    struct Result {
        int affected{0};
    };
};

struct ResumeAllPacket : WithPacketPayload<EmptyPayload> {
    static constexpr std::string_view kMethod = "resumeAll";
    struct Result {
        int affected{0};
    };
};

// ---- 事件 packet ----

struct TaskCreatedPacket {
    static constexpr std::string_view kEvent = "taskCreated";
    using PayloadType                        = TaskSnapshotPayload;
};

struct TaskUpdatedPacket {
    static constexpr std::string_view kEvent = "taskUpdated";
    using PayloadType                        = TaskSnapshotPayload;
};

struct TaskRemovedPacket {
    static constexpr std::string_view kEvent = "taskRemoved";
    using PayloadType                        = TaskIdPayload;
};

struct TaskErrorPacket {
    static constexpr std::string_view kEvent = "taskError";
    using PayloadType                        = TaskErrorPayload;
};

struct TaskFinishedPacket {
    static constexpr std::string_view kEvent = "taskFinished";
    using PayloadType                        = TaskFinishedPayload;
};

} // namespace edm::ws