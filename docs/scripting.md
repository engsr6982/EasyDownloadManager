# 脚本扩展开发

EasyDownloadManager 的扩展系统基于生命周期钩子：脚本运行于内置的 QuickJS 引擎
（经 [jspp](https://github.com/engsr6982/jspp) 绑定），监听下载引擎各阶段的事件，
通过改写事件对象控制下载行为，或接收任务通知。

每个脚本一个独立引擎，单个脚本的异常只写入自己的虚拟控制台，不影响其他脚本与下载器本体。
全部脚本代码在主线程执行；`beforeFetchMeta` 与 `afterFetchMeta` 的 handler 可返回
Promise 做异步改写，其余钩子为同步。

## 安装

脚本目录默认为 `%LOCALAPPDATA%/EasyDownloadManager/scripts`，可用环境变量 `EDM_SCRIPT_DIR` 覆盖。
将包含 `manifest.json` 的脚本文件夹放入该目录，或在插件中心导入；卡片可启用、禁用、
重载、卸载脚本（卸载会物理删除插件目录），虚拟控制台实时显示日志与异常。

## Manifest

```
scripts/
  my-script/
    manifest.json
    index.js
```

| 字段 | 必填 | 说明 |
|---|---|---|
| `name` | 是 | 脚本名，与文件夹名一致；字符集 `[A-Za-z0-9._-]` |
| `type` | 是 | 固定为 `"edm-script"` |
| `entry` | 是 | 入口 ES module 路径，相对脚本目录，不允许绝对路径与 `..` |
| `version` | 是 | 语义化版本，如 `"1.2.0"` 或 `"1.0.0-beta.1"` |
| `author` | | 作者 |
| `description` | | 一句话描述，展示在插件中心卡片 |
| `homepage` | | 主页链接 |
| `icon` | | 卡片图标，相对脚本目录的 posix 路径（如 `"assets/icon.png"`）；文件缺失时回退默认图标 |
| `minAppVersion` | | 最低应用版本要求，不满足则拒绝加载 |

校验失败时该脚本不加载，错误写入应用日志。JSON Schema 位于 `docs/manifest.schema.json`，
manifest 携带 `"$schema": "../../../docs/manifest.schema.json"` 即可在 IDE 中获得补全与校验；
宿主忽略未在 schema 中声明的字段，schema 校验仅用于 IDE 提示。

## 入口与生命周期

入口模块 `export default` 一个类，宿主创建实例并按需调用三个可选方法：

```js
export default class MyScript {
    onLoad() {}    // 模块加载完成，尚未启用；钩子注册位于 onEnable
    onEnable() {}  // 注册事件监听、定时器等
    onDisable() {} // 反注册资源；监听器 id 随引擎销毁自动失效
}
```

启用状态持久化于配置，未列出的脚本默认启用。重载脚本会先调用 `onDisable` 再重新走加载流程。

## 钩子

任务生命周期分四个阶段：创建（`beforeCreateTask`），准备（`beforeFetchMeta` 元信息探测、
`afterFetchMeta` 文件名解析），传输（`taskStarted`、`taskProgress`、`taskPaused`、`taskResumed`），
终态（`taskCompleted`、`taskFailed`、`taskCanceled`、`taskRemoved`）。

### 可变钩子

可变钩子在派发期间可改写事件字段，赋值即生效；除 `proxyUrl`（会话级，不持久化）外，
改写结果持久化，断点续传与重试沿用。

| 钩子 | 触发点 | 异步 | 字段 |
|---|---|---|---|
| `beforeCreateTask` | 任务创建、落库前 | 否 | 可改写 `url` `fileName` `saveDir` `bandLimit` `threadCount` `userAgent` `origin` `referer` `cookie` `headers` `useProxy` `proxyUrl` `overwriteExisting`；只读 `taskId`（创建时尚未分配，恒为 `constants.invalidTaskId`） |
| `beforeFetchMeta` | 元信息探测前 | 是 | 可改写 `url` `userAgent` `origin` `referer` `cookie` `headers` `useProxy` `proxyUrl`；只读 `taskId`。URL 改写点：页面链接改直链后持久化 |
| `afterFetchMeta` | 文件名解析后、磁盘预分配前 | 是 | 可改写 `fileName`（重命名点，改写后引擎据此写盘）与 `url` `userAgent` `origin` `referer` `cookie` `headers`；只读 `taskId` `finalUrl` `supportRange` `fileSize` `contentType` `etag` `lastModified` `contentDisposition` `md5` |

`cancel(reason)` 停止向后续监听器派发：`beforeCreateTask` 阶段使创建失败并提示原因，
准备阶段使任务进入 Canceled，原因写入任务。

异步改写：`beforeFetchMeta`/`afterFetchMeta` 的 handler 返回 Promise 时宿主等待其完成，
超时（`hookTimeoutMs`，默认 10 秒）按已改写内容继续并告警；`beforeCreateTask` 仅支持同步。
事件对象在派发结束后失效，异步闭包中再访问会抛出异常。

### 通知钩子

通知钩子只读，字段为 `TaskEvent`：`taskId` `url` `fileName` `error`（仅 failed）、
`downloaded` `total` `speed` `percent`（未知为 `constants.unknownPercent`）。

| 钩子 | 触发点 |
|---|---|
| `taskStarted` / `taskResumed` | 进入 Running |
| `taskProgress` | 每秒汇总，仅活动任务 |
| `taskPaused` | 进入 Paused |
| `taskCompleted` / `taskFailed` / `taskCanceled` | 进入终态 |
| `taskRemoved` | 任务从列表删除 |

### 派发语义

监听器按 Priority（数值小者先）、脚本加载顺序、注册顺序依次调用。单个监听器抛出的
异常写入该脚本控制台（含 stacktrace），不影响后续监听器。恢复下载会再次触发准备阶段
钩子，改写逻辑需保持幂等。

## API

原生模块 `"edm"` 提供以下成员，完整类型见 [edm.d.ts](edm.d.ts)（TypeScript 工程引用可得补全）。

| 成员 | 说明 |
|---|---|
| `events` | `listen(event, handler, priority?)` 返回监听器 id，`unlisten(id)` 移除 |
| `commands` | `register(title, description, handler)` 注册命令面板条目并返回句柄，`unregister(id)` 移除；引擎卸载时自动清理 |
| `logger` | `info/warn/error/debug(fmt, ...args)`，`{}` 占位符格式化，写入虚拟控制台 |
| `tasks` | `get(id)` / `getAll()` / `pause(id)` / `resume(id)` / `cancel(id)` |
| `fs` / `path` | 文本文件读写与 posix 路径工具；相对路径以脚本目录为根 |
| `http` | `get(url, options?)` / `head(...)`，返回 Promise，响应体上限 16MB |
| `utils` | `formatBytes` / `formatSpeed`，与主程序显示格式一致 |
| `constants` | `invalidTaskId` / `unknownFileName` / `unknownFileSize` / `unknownPercent` |
| `notify` | 托盘通知，`notify(message, { title?, level? })`（顶层函数） |
| `setTimeout` 等 | 定时器四函数（顶层），回调在主线程执行 |
| `Priority` 等枚举 | `Priority` / `TaskState` / `NotifyLevel` |

## 最小示例

```js
import { events, logger } from "edm";

export default class {
    onEnable() {
        events.listen("beforeCreateTask", (ev) => {
            if (ev.url.includes("example.com")) {
                ev.useProxy = true;
                logger.info("已为 example.com 启用代理");
            }
        });
    }
}
```

更多完整示例见 [examples/scripts](../examples/scripts)：download-logger（通知钩子）、
url-rewriter（URL 与文件名改写）、proxy-demo（按域名启用代理）、auto-retry（失败自动重试）、
domain-blocker（黑名单拦截）、dedupe-guard（重复任务拦截）。
