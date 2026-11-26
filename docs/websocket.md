# WebSocket 协议

浏览器扩展与 EasyDownloadManager 之间的报文契约。服务器随桌面应用启动，供扩展创建与
管理下载任务、订阅任务状态。协议以文本帧承载 UTF-8 JSON，字段名区分大小写。

## 连接与鉴权

服务器默认监听 `127.0.0.1:26111`，只绑定回环地址；设置中开启局域网访问后监听全部网卡。
启动时默认启用。端口、启用开关与访问令牌位于设置页；令牌首次读取时随机生成并持久化，
留空表示不校验。

连接建立后必须在 10 秒内完成鉴权，否则服务端断开。首条消息必须是 `auth` 请求，未鉴权
的会话调用其他方法回错误信封。令牌不匹配时回错误信封并立即断开连接。多个客户端可以同时
连接：响应只回给发起请求的会话，事件广播给全部已鉴权会话。更换令牌会断开全部现有会话。

## 信封

报文分两类，客户端以 `id` 字段区分：带 `id` 的是 RPC 响应，只回给发起该请求的会话；
带 `event` 且无 `id` 的是广播事件，单向通知全体已鉴权会话，不对应任何请求，也不需要
回执。

```json
{ "id": 1, "method": "getTask", "params": { "taskId": 12 } }
{ "id": 1, "ok": true, "result": { "task": { "id": 12 } } }
{ "id": 1, "ok": false, "error": "任务不存在: 12" }
{ "event": "taskCreated", "data": { "id": 12, "state": "Pending" } }
```

请求的 `id` 由客户端在自身会话内生成并维护，服务端在响应中原样回显。`id` 的作用域限于
单条连接，多个客户端各自独立编号，互不影响；响应绝不串到其他会话，事件绝不携带 `id`。
成功响应只带 `result`，失败响应只带 `error`。

报文不是合法的协议消息、方法未知、未鉴权或 `params` 解析失败时，向发起会话回复错误信封；
能读出 id 的请求回显该 id，无法解析的报文以 0 回复。

```ts
/** 请求信封，仅发起会话可见其响应 */
interface RequestEnvelope {
  /** 客户端在本会话内生成的请求 id，响应原样回显 */
  id: number;
  /** 方法名，见下文各数据包 */
  method: string;
  /** 方法参数；无参数的方法可省略 */
  params?: object;
}

/** 成功响应信封，单播回发起会话 */
interface SuccessEnvelope<T> {
  id: number;
  ok: true;
  result: T;
}

/** 失败响应信封，单播回发起会话 */
interface ErrorEnvelope {
  id: number;
  ok: false;
  /** 错误文本，供展示，客户端不应按文本分支 */
  error: string;
}

/** 事件信封，广播给全体已鉴权会话，无 id */
interface EventEnvelope<T> {
  /** 事件名，见下文各事件数据包 */
  event: string;
  data: T;
}
```

## 任务快照

```json
{
  "id": 12,
  "url": "https://example.com/archive.zip",
  "fileName": "archive.zip",
  "saveDir": "D:\\Downloads",
  "state": "Running",
  "fileSize": 104857600,
  "downloaded": 52428800,
  "speed": 4194304,
  "threadCount": 8
}
```

任务查询与任务事件共用此形状。`Preparing` 覆盖元信息探测与磁盘预分配阶段；进度按
`downloaded / fileSize` 计算。

```ts
type TaskState = "Pending" | "Preparing" | "Running" | "Paused" | "Canceled" | "Finished" | "Failed";

interface TaskSnapshot {
  /** 任务 id，创建后不变 */
  id: number;
  /** 下载地址 */
  url: string;
  /** 文件名 */
  fileName: string;
  /** 保存目录 */
  saveDir: string;
  /** 任务状态 */
  state: TaskState;
  /** 文件总字节数，未知为 -1 */
  fileSize: number;
  /** 已下载字节数 */
  downloaded: number;
  /** 当前速度，字节每秒；任务不在引擎中时为 0 */
  speed: number;
  /** 并发线程数 */
  threadCount: number;
}
```

## auth - 鉴权

```json
{ "id": 1, "method": "auth", "params": { "token": "7c1e5d2a9b4f" } }
{ "id": 1, "ok": true, "result": { "authenticated": true } }
```

令牌不匹配时回错误信封并断开连接。

```ts
interface AuthParams {
  /** 访问令牌，设置页可查看 */
  token: string;
}

interface AuthResult {
  /** 鉴权通过 */
  authenticated: boolean;
}
```

## ping - 连通性探测

```json
{ "id": 2, "method": "ping" }
{ "id": 2, "ok": true, "result": { "pong": true } }
```

用于保活与延迟测量，无参数。

```ts
interface PingResult {
  pong: boolean;
}
```

## createTask - 创建下载任务

```json
{
  "id": 3,
  "method": "createTask",
  "params": { "url": "https://example.com/archive.zip", "saveDir": "D:\\Downloads", "useProxy": false }
}
{ "id": 3, "ok": true, "result": { "taskId": 12 } }
```

任务创建后广播 `taskCreated`，元信息探测与下载在后台进行。URL 为空、脚本钩子取消或
落库失败时回错误信封。

```ts
interface CreateTaskParams {
  /** 下载地址，必填 */
  url: string;
  /** 保存目录，缺省沿用设置中的保存目录 */
  saveDir?: string;
  /** 是否走代理，缺省沿用设置中的代理开关 */
  useProxy?: boolean;
}

interface CreateTaskResult {
  /** 新任务 id */
  taskId: number;
}
```

## listTasks - 列出全部任务

```json
{ "id": 4, "method": "listTasks" }
{
  "id": 4,
  "ok": true,
  "result": {
    "tasks": [
      {
        "id": 12,
        "url": "https://example.com/archive.zip",
        "fileName": "archive.zip",
        "saveDir": "D:\\Downloads",
        "state": "Finished",
        "fileSize": 104857600,
        "downloaded": 104857600,
        "speed": 0,
        "threadCount": 8
      }
    ]
  }
}
```

包含全部持久化任务，无参数。

```ts
interface ListTasksResult {
  tasks: TaskSnapshot[];
}
```

## getTask - 查询单个任务

```json
{ "id": 5, "method": "getTask", "params": { "taskId": 12 } }
{
  "id": 5,
  "ok": true,
  "result": {
    "task": {
      "id": 12,
      "url": "https://example.com/archive.zip",
      "fileName": "archive.zip",
      "saveDir": "D:\\Downloads",
      "state": "Running",
      "fileSize": 104857600,
      "downloaded": 52428800,
      "speed": 4194304,
      "threadCount": 8
    }
  }
}
```

任务不存在时回错误信封。

```ts
interface TaskIdParams {
  taskId: number;
}

interface GetTaskResult {
  task: TaskSnapshot;
}
```

## pauseTask - 暂停任务

```json
{ "id": 6, "method": "pauseTask", "params": { "taskId": 12 } }
{ "id": 6, "ok": true, "result": { "taskId": 12 } }
```

暂停在后台执行，响应仅表示请求已受理。任务不在引擎中（已完成或从未派发）时按成功处理。

```ts
interface TaskActionResult {
  /** 请求中的 taskId 回显 */
  taskId: number;
}
```

## resumeTask - 恢复任务

```json
{ "id": 7, "method": "resumeTask", "params": { "taskId": 12 } }
{ "id": 7, "ok": true, "result": { "taskId": 12 } }
```

已完成的任务拒绝恢复并回错误信封，应改用 `redownloadTask`。重启后未加载到引擎的任务按
断点续传重新派发。参数与结果形状同 `pauseTask`。

## cancelTask - 取消任务

```json
{ "id": 8, "method": "cancelTask", "params": { "taskId": 12 } }
{ "id": 8, "ok": true, "result": { "taskId": 12 } }
```

取消在后台执行，引擎拒绝时经 `taskError` 事件通知。参数与结果形状同 `pauseTask`。

## deleteTask - 删除任务

```json
{ "id": 9, "method": "deleteTask", "params": { "taskId": 12, "deleteFiles": true } }
{ "id": 9, "ok": true, "result": { "taskId": 12 } }
```

删除在后台执行，响应返回后任务可能短暂仍可查询；删除完成时广播 `taskRemoved`，磁盘
清理失败经 `taskError` 通知。

```ts
interface DeleteTaskParams {
  taskId: number;
  /** 同时删除最终文件与临时文件，缺省 false 仅移除任务记录 */
  deleteFiles?: boolean;
}
```

## redownloadTask - 重新下载任务

```json
{ "id": 10, "method": "redownloadTask", "params": { "taskId": 12 } }
{ "id": 10, "ok": true, "result": { "taskId": 12 } }
```

清除临时文件与进度后以原 id 重新入队，最终文件在下载校验通过后覆盖。任务不存在时回
错误信封。参数与结果形状同 `pauseTask`。

## pauseAll - 暂停全部活动任务

```json
{ "id": 11, "method": "pauseAll" }
{ "id": 11, "ok": true, "result": { "affected": 2 } }
```

暂停 `Running` 与 `Preparing` 状态的任务，无参数。

```ts
interface AffectedResult {
  /** 实际受理的任务数 */
  affected: number;
}
```

## resumeAll - 恢复全部暂停任务

```json
{ "id": 12, "method": "resumeAll" }
{ "id": 12, "ok": true, "result": { "affected": 1 } }
```

恢复 `Paused` 状态的任务，无参数。结果形状同 `pauseAll`。

## taskCreated - 任务创建通知

```json
{
  "event": "taskCreated",
  "data": {
    "id": 12,
    "url": "https://example.com/archive.zip",
    "fileName": "archive.zip",
    "saveDir": "D:\\Downloads",
    "state": "Pending",
    "fileSize": -1,
    "downloaded": 0,
    "speed": 0,
    "threadCount": 8
  }
}
```

任务落库后广播，此时状态为 `Pending`。

```ts
interface TaskCreatedEvent {
  event: "taskCreated";
  /** 创建后的任务快照 */
  data: TaskSnapshot;
}
```

## taskUpdated - 任务更新通知

```json
{
  "event": "taskUpdated",
  "data": {
    "id": 12,
    "url": "https://example.com/archive.zip",
    "fileName": "archive.zip",
    "saveDir": "D:\\Downloads",
    "state": "Running",
    "fileSize": 104857600,
    "downloaded": 52428800,
    "speed": 4194304,
    "threadCount": 8
  }
}
```

任务状态变化或解析出最终文件名时广播。进度变化不广播，客户端按需轮询 `getTask`。

```ts
interface TaskUpdatedEvent {
  event: "taskUpdated";
  /** 更新后的任务快照 */
  data: TaskSnapshot;
}
```

## taskRemoved - 任务移除通知

```json
{ "event": "taskRemoved", "data": { "taskId": 12 } }
```

任务从数据库删除后广播。

```ts
interface TaskRemovedEvent {
  event: "taskRemoved";
  data: { taskId: number };
}
```

## taskError - 任务错误通知

```json
{ "event": "taskError", "data": { "taskId": 12, "message": "取消失败: task not found" } }
```

引擎回传的错误与后台操作失败均经此事件通知。任务状态变化以 `taskUpdated` 事件为准。

```ts
interface TaskErrorEvent {
  event: "taskError";
  data: {
    taskId: number;
    /** 错误文本，已写入任务记录 */
    message: string;
  };
}
```

## taskFinished - 任务终态通知

```json
{
  "event": "taskFinished",
  "data": { "taskId": 12, "state": "Finished", "fileName": "archive.zip", "errorMsg": "" }
}
```

任务进入 `Finished`、`Failed` 或 `Canceled` 时广播。`errorMsg` 仅在失败时非空。

```ts
interface TaskFinishedEvent {
  event: "taskFinished";
  data: {
    taskId: number;
    /** 终态 */
    state: Extract<TaskState, "Finished" | "Failed" | "Canceled">;
    /** 最终文件名 */
    fileName: string;
    /** 失败原因，其余状态为空串 */
    errorMsg: string;
  };
}
```