# AGENTS.md

面向 AI agent 与维护者的项目指引。用户文档见 README.md 与 docs/scripting.md。

## 项目概览

Qt6/C++23 的 Windows 多线程下载器，Fluent 风格 QML 界面，内建脚本扩展系统。
单顶层 CMake 定义四个目标：

- `src-common`（接口库 `edm-common`）：跨层通用代码（`expected.h` 的 `Expected`/`Error`、
  `Global.h` 的领域值类型与枚举、`utils/`），仅依赖 fmt，无 Qt
- `src-downloader`（静态库）：下载引擎全部逻辑（curl 多线程分片、断点续传、持久化），无 Qt
- `src-downloader-cli`（test/cli）：手动 CLI 测试工具
- `EasyDownloadManager`（src/）：Qt 应用，链 src-downloader 与 edm-common，持有 AppService、
  配置、托盘与 QML 桥接层

src-ui/ 为 QML 界面源码，经根 CMake 的 `qt_add_qml_module` 接入；test/scripting 为脚本系统
E2E，test/server 为 Node HTTP 测试服务器。依赖经 vcpkg 清单（curl http2+ssl、SQLiteCpp、fmt、
spdlog、glaze、magic-enum），Qt6 经 `CMAKE_PREFIX_PATH` 提供，quickjs-ng 与 jspp 经
FetchContent 拉取（跟踪 main 分支，更新需删 `_deps` 重新配置）。

## 构建

Windows，MSVC 2022，CMake + Ninja。工具链参数与 `.vscode/settings.json` 保持一致：

```sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_PREFIX_PATH=<Qt>/6.8.3/msvc2022_64
```

MSVC 运行时强制 DLL 变体（`MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`），vcpkg 二进制必须匹配。
`WIN32_EXECUTABLE` 为 FALSE 使 qDebug 输出可见；Debug 构建定义 `EDM_DEBUG`；
`src-downloader` 目标定义 `SPDLOG_FMT_EXTERNAL`、`CURL_STATICLIB`、`NOMINMAX`。

## 验证

- CLI：`node test/server/server.js` 后运行
  `src-downloader-cli --url <url> --output-dir <dir> [--threads N] [--retry N] [--timeout-seconds N] [--user-agent X] [--proxy url]`，
  目标地址 `http://127.0.0.1:3187/file`
- 脚本 E2E：启动 `test/server/server.js`（`TEST_FILE_SIZE=33554432 TEST_FILE_NAME=e2e-test.bin`，
  路由为 `/file` 与 `/file-range`），设 `EDM_SCRIPT_DIR=<repo>/.tmp/test-scripts`、
  `EDM_TEST_URL=http://127.0.0.1:3187/file`、`EDM_TEST_DIR` 后运行 `edm-scripting-smoke.exe`，
  退出码 0 为全部通过。构建：`cmake --build cmake-build-debug-clang-cl --target edm-scripting-smoke --target EasyDownloadManager`
- GUI：懒加载页启动后不导航则页面 QML 错误不暴露；临时将 `App.qml` viewLoader 默认 source
  切到目标页，跑 GUI 后全量检查日志（不过滤），验后还原
- 构建与拉取需网络时用代理 `http://127.0.0.1:7890`

## 架构

### 下载引擎（src-downloader）

分层、回调驱动、持久 worker 池：worker 线程随任务常驻（世代号驱动重入，
pause 驻留而非退出），curl 句柄线程内持久复用（连接缓存跨分片保活）。

- `Dispatcher`：顶层编排器，互斥锁下持有 activeTasks_，经 `ITaskObserver*`（生命周期须长于
  dispatcher）接收状态/错误/文件名变更；入口 createTask/pauseTask/resumeTask/cancelTask/updateSpeeds
- `DownloadTask`：持有活动任务，持久 worker 池（任务级复用，workerLaunchCount 观测
  线程创建批次），`.meta` sidecar 记录分片进度供续传；不可拷贝不可移动
  （enable_shared_from_this）
- `DownloadWorker`：每分片一个 curl 传输，经共享原子标志接收暂停/取消
- `DownloadState`：状态/字节的原子量与 shared_mutex 保护的字符串，GUI 轮询的实时数据源
- `TaskContext`（DownloadTypes.h）：curl 会话工厂，由 TaskModel 构造，携带代理/UA/头/线程/带宽/重试
- `TaskLogger`：按 TaskId 的 spdlog 实例，只用 `EDM_LOG_*(taskId, fmt, ...)` 宏
- `schema::ChunkMetaFile`：glaze BEVE 二进制序列化的 `.meta` 文件，`Envelope<T>` 附魔数/版本/校验和

### GUI 宿主（src/）

界面为 Qt Quick/QML，Widgets 仅用于 QApplication、QSystemTrayIcon、QMenu。QML 风格为
Basic（`QQuickStyle::setStyle`），自定义 Fluent 组件在 src-ui/Src/fluent/。

QML 桥接层（src/qml/，经 `EdmApplication::initialize()` 注册为单例）：

- `QmlTaskListModel`：核心桥接，AppService 之上的 QAbstractListModel；动作委托给 service，
  行由 service 信号驱动；1 秒定时器调用 updateSpeeds 并发 dataChanged（任务列表行）；
  `getTaskDetail(id)` 惰性创建 `QmlTaskDetail`（实时速率、ETA、错误串、rangesModel 线程状态）。
  任务详情实时数据为拉取式：抽屉可见期的定时器调用 `detail.pullLive()`（内部
  refreshDetailById 从引擎读取），不可见不拉取，不做全局定时推送
- `QmlSettings`：包装 EdmConfig 单例，QML 双向绑定设置项
- `QmlController`：应用级动作（最小化到托盘、打开文件/文件夹、剪贴板、无边框窗口外观
  `installWindowFrameless`）
- `QmlScriptManager` / `ScriptLogModel`：脚本扩展的展示模型（见下）

核心装配：

- `EdmApplication`：组合根，initialize() 创建 AppService、QML 引擎、注册单例、托盘；额外注册
  C++ QML 类型入口 `qml_register_types_edm_qml_ui()`（见踩坑 12）
- `AppService`：任务域唯一所有者，持有 DownloadDatabase + Dispatcher + ITaskObserver 实现，
  全部任务操作在此并返回 `Expected<>`；观察者把引擎状态回写数据库并发 Qt 信号（跨线程自动排队）。
  新任务逻辑加在这里，不加在 QML 桥接层
- `DownloadDatabase`：SQLiteCpp 持久化（downloads 与 headers 两表），互斥锁保护（观察者
  可能从引擎线程调用）
- `TaskModel`/TaskModelConversion：数据库数据结构与引擎 TaskContext 的双向转换

数据流：

- 创建：NewTaskDialog 到 TaskListModel.createTask 到 AppService（建 TaskModel、落库、
  toTaskContext、Dispatcher::createTask、发 taskCreated）到模型插行
- 实时：引擎线程经观察者 applyContextToModel + 落库，发 taskStateChanged 等信号，行即时刷新
- 持久化：worker 进度到 .meta sidecar；运行时状态经观察者入 SQLite；配置经 QmlSettings 到 EdmConfig

### 脚本扩展（src/scripting）

基于生命周期钩子的扩展系统：脚本运行于每脚本独立的 jspp(QuickJS) 引擎，监听下载引擎事件，
经改写事件对象控制下载行为或接收通知。已实现 11 个钩子（可变钩子支持同步与 Promise 异步改写，
取消经 cancel(reason) 贯通任务终态）、events/logger/tasks/fs/path/http/utils/commands/constants/notify
与顶层定时器绑定、插件中心、命令面板、虚拟控制台、manifest 校验与导入。

- `src/scripting/api/`：脚本可见的 C++ 接口与数据模型（Console/Events/Manifest/Fs/Http/
  HostServices），Qt-free
- `src/scripting/binding/`：唯一绑定文件 ScriptBindings，纯声明加必要胶水
- `src/scripting/engine/`：ScriptEngineState（每脚本引擎状态）、ScriptEventBus（钩子派发）、
  ScriptingBootstrap（扫描加载编排）
- `src/scripting/qt/`：ScriptQtHost，ScriptHostServices 的 Qt 实现（QTimer 定时器与泵）
- `src/qml/`：QmlScriptManager 与 ScriptLogModel 展示模型
- 分层禁令：src-downloader 不含脚本与 Qt；src/scripting 不含 Qt；Qt 仅在 qt/、src/qml、
  AppService/EdmApplication

实现要点：

- 加载流程：QML 就绪后扫描脚本目录（按目录名排序），逐个校验 manifest；每个启用脚本创建
  独立引擎并注册原生模块，loadModule 后实例化 default export 类，依次调用 onLoad/onEnable；
  任一步失败该脚本标记为错误并卸载，不影响其他脚本
- 线程模型：脚本代码只在主线程执行。可变钩子由引擎后台线程经"主线程投递 + 条件变量等待
  （带超时）"派发，`hookTimeoutMs` 约束单钩子最长阻塞；通知钩子经 QueuedConnection 派发
- 资源回收：引擎创建点以 make_shared 挂载 EngineOwnData（`Engine::setData`），jspp 保证其
  析构先于 runtime 释放，监听器、定时器、挂起请求、长期句柄在析构中统一回收；长期持有的
  脚本回调使用 TrackedGlobal，引擎销毁自动置空
- http 能力：响应体上限 16MB，超出报错；结果经 JobQueue::postTask 回注主线程泵，与钩子
  等待循环、低频泵（50ms）两种消费方式兼容
- 持久化：可变钩子改写的 url/userAgent/fileName/headers/origin/referer/cookie 落库
  （customHeaders 为 JSON 对象列），断点续传与重试沿用；proxyUrl 为会话级不持久化

## 约定

- C++23。C++ 改动经 clang-format（仓库 .clang-format，LLVM 风格）后提交；.clang-tidy 已配置
  （bugprone/cert/modernize/performance），不引入新警告；clangd 需要构建目录的
  compile_commands.json
- 注释与标识符以简中为主，编辑时跟随所在文件的语言
- 错误处理：引擎全程 `Expected`（`makeStringError`/`makeError`/`forwardError`），
  异常不穿越 src-downloader
- 分层边界：新引擎代码进 src-downloader 且不得包含 Q* 头；桥接对象（Qml*）在 src/qml/；
  QML 源在 src-ui/，build/ 下的 QML 是拷贝产物勿改
- 禁止 emoji 与装饰符号出现在代码、注释、UI；仅简中、ASCII 与 Fluent 图标字体
- 注释与文档禁止引用作为设计参考的特定领域中小项目；大型基础设施项目与 cmake 拉取的
  依赖不受限（如 Node.js path.posix、jspp、Qt）
- 注释只做第三人称技术描述：需求由来、修复历史、行为对比一律不写；接到的要求不得转述为注释
- 禁止说教、命令式语气、模糊指代与长篇论述；一句说清，能删则删
- 文档按受众分离：docs/scripting.md 面向脚本作者，本文件面向 agent 与维护者，实现细节不进
  用户文档；README 面向使用者介绍功能，不穷举清单
- markdown 散文优先，表格、列表与嵌套仅用于真正结构化的数据；参照大型基础设施项目的文档写法
- 脚本 API 以 docs/edm.d.ts 为唯一权威参考，其余文档不手抄 API 签名
- 已删除或已废弃的内容不在文档中留痕
- 全部文本不得互相矛盾；接口现状变更时同步所有引用处（README、d.ts、示例、注释）
- 图标字形以 \uXXXX 转义书写并附字形名注释，不写入字面 PUA 字符

设计偏好：

- 命令式事件方法：`cancel()/cancel(reason)/cancelReason()/setCancelled(bool)/isCancelled()`，
  事件状态一律以方法暴露，不导出只读属性
- options 参数用 plain object 加 TypeConverter 特化整体 toCpp
- 存储统一走 fs 与脚本侧 JSON 序列化
- 脚本入口 `export default class` 加 `onLoad/onEnable/onDisable`
- 元对象（ClassMeta/EnumMeta/ModuleMeta）一律 `edmModule()` 内函数级 static，jspp 按指针
  存储，传临时必崩
- 脚本 API 走 static class 分组，notify 与定时器四函数为模块顶层；禁顶层散函数堆积
- 魔术值经 constants 导出，脚本侧禁止硬编码

## 踩坑记录

1. jspp `export_enum/export_class` 按指针存元：传临时（如 `export_enum(bindEnum<E>(...))`）
   会崩溃或悬垂，函数级 static 解决。
2. `inherit` 要求基类暴露给 JS 且 `hasConstructor()`；`ctor(nullptr)` 的类不能作基类。失败报
   "Base class not registered: <派生类名>"（消息指向派生类，有误导）。
3. `BlockingQueuedConnection` 在应用退出时主线程停止调度，后台线程永久卡死在等待；改 Queued
   投递加 condition_variable 超时等待。
4. http 工作线程回调不能经 Qt 队列（可变钩子 Promise 等待循环不处理 Qt 事件，resolve 永远
   送达）；改经 `EngineOwnData` 托管队列，两处泵消费。
5. Promise 等待期间不持有 EngineScope（跨线程 resolve 需要）；jspp::Exception 的
   message/stacktrace 须在 EngineScope 存活期内消费，catch 放 scope 内。
6. MSVC CRT assert 默认弹模态对话框，卡死无人值守运行；测试入口 `_CrtSetReportMode` 重定向
   stderr。
7. GUI 懒加载页（Loader）：启动后不导航则页面 QML 错误不暴露；验证时临时将 `App.qml` 默认页
   切到目标页，跑全量日志检查，验后还原。
8. GUI 链接失败 "permission denied" 为 exe 仍在运行，先 taskkill。
9. `TaskContext.url` 已去 const（beforeFetchMeta 改写需要）；DB 用 UPSERT 保 headers 行
   （INSERT OR REPLACE 会级联删 headers）。
10. bash heredoc 传 python 反斜杠转义不可靠（CRLF/转义丢失），多行文件修改用 Edit、perl
    -0777 或脚本文件。
11. `ScriptEngineState::services()` 返回裸指针（接口方法非 const），调用方需判 services_ 非空
    （initialize 前 QML 可能触发 discoverScripts）。
12. `qt_add_qml_module` 直挂 exe 且 Qt 动态链接时，生成的 `qml_register_types_edm_qml_ui()`
    无人调用（无静态插件导入路径），QML_ELEMENT 类型（ScriptLogModel 等）在 QML 中全部不可见，
    报 "ReferenceError: XxxModel is not defined"。须在 `_initializeQml` 里加载 QML 前手动调用
    （EdmApplication.cc 顶部 extern 声明，置于全局作用域）。
13. 示例脚本 API 变更后，`%LOCALAPPDATA%/EasyDownloadManager/scripts` 里的旧副本不会自愈，
    GUI 报 "Could not find export 'listen' in module 'edm'"（QuickJS 模块实例化期错误），重新
    拷贝 `examples/scripts/*` 即可。
14. MCP/短超时构建被掐断会留僵尸 ninja 进程（锁 `.ninja_deps`，configure 报 "failed
    recompaction: Permission denied"）且可能杀断 configure 内的 vcpkg manifest install
    （`vcpkg_installed` 只剩 share/）。`taskkill //IM ninja.exe //F` 清理进程后，经 CLI
    `cmake --build` 长超时构建。
15. 自定义 QML role 返回 invalid QVariant 时 QML 侧得到 undefined，赋值报 "Unable to assign
    [undefined] to QString"。可选 string role（如 icon）缺值时必须返回空 `QString`，不是 `{}`。
16. `Q_INVOKABLE` 参数用指针类型（如 `QWindow*`）时头文件必须 include 完整类型（仅前向声明会
    触发 qmetatype 静态断言 "Pointer Meta Types must point to fully-defined types"）。
17. 绑定层 `requires` 探测失败是静默的：`Cancellable` 缺 `cancel(reason)` 重载时探测恒 false，
    `defEventClass` 整个分支被丢弃，脚本侧事件方法不存在且无编译错误。d.ts 声明与绑定导出须
    成对核对，cancelReason 方法与 cancel 可选参数曾同时缺失。
18. `_deps` 内的 FetchContent 副本被中断的构建/配置破坏后（如半克隆、.git 缺失），增量
    configure 无法自愈；删 `_deps` 重新配置，GitHub 拉取需走代理环境变量。
20. C++ 侧 `QIcon`/`QFile` 按文件名处理资源路径，只认 ":/..." 短形式；"qrc:/..." 两斜杠形式
    只有 QML URL 层能解析，C++ 传入结果为空且无报错。QML `Window` 类型无 icon 属性，须从
    C++ `setWindowIcon` 设置。
21. `AppService::taskSnapshots` 曾在 forEachTask 持锁回调里嵌套调用 taskSnapshot（内部对同一
    数据库互斥锁二次加锁），同线程触发 EDEADLK 被 catch 吞掉，tasks.getAll 恒为空且无异常
    浮出。持锁遍历内复用无锁的 makeSnapshot(model)；此类嵌套加锁在 catch-all 环境下静默
    失效，E2E 必须覆盖 tasks.getAll 的数据可见性。
22. worker 线程默认栈仅 1MB：FileVerifier 曾在栈上放 1MB MD5 缓冲，finalizeTask 在 worker
    线程执行时栈溢出崩溃（bash 报 exit 127）。大缓冲一律堆分配。
23. clang-format 会按字母序重排 include，windows.h 被排到 bcrypt.h 之后导致 LONG/ULONG
    未定义；须用 clang-format off/on 钳住顺序敏感的 include 组。
19. QML 侧无边框窗口（`installWindowFrameless`）：窗口隐藏时 Qt 可能销毁平台窗口（HWND 变化），
    chrome 须在每次 `visibleChanged(true)` 时重新施加，否则重开窗口阴影丢失；动态改 flags
    （如 StaysOnTop）后需 hide 加 show 才生效。

## 未决项

- fs 无沙箱：绝对路径可达任意位置。
- 事件 `proxyUrl` 改写为会话级，不持久化。
- 引擎卸载竞态时挂起的 http 任务随队列销毁（单任务指针量级泄漏，仅该路径）。
- jspp 跟踪 main 分支（`FETCHCONTENT_UPDATES_DISCONNECTED=ON`），更新需删 `_deps` 重新拉取。
