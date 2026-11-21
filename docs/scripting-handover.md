# 脚本扩展系统交接单

## 当前状态

- 全量构建零错误；E2E（`test/scripting/edm-scripting-smoke`，配合 `test/server/server.js`）五项 PASS：下载完成、通知钩子、afterFetchMeta 改写、异步 Promise 改写、错误隔离。
- GUI 启动无 QML 错误（验证方式见"验证手段"）。
- 已实现：11 个生命周期钩子、fs/path/http/tasks/utils 原生模块与 notify、setTimeout 等顶层定时器函数、每脚本独立 jspp 引擎、虚拟控制台（独立无边框 Fluent 窗口，黑色终端风日志区）、插件中心（启停/重载/导入/更多操作菜单，卡片支持 manifest.icon 自定义图标+圆角遮罩）、manifest 校验与导入（可选 icon 字段；容忍未知字段如 $schema；schema 见 docs/manifest.schema.json）、示例扩展 6 个（examples/scripts/，含 auto-retry/domain-blocker/dedupe-guard）、URL/文件名改写持久化（DB `customHeaders` 列）。
- QML 侧无边框窗口复用：`AppController.installWindowFrameless(QWindow)`（QmlController，FramelessShadowFilter 实例按窗口托管，含边缘拖拽缩放）。窗口隐藏时 Qt 可能销毁平台窗口（HWND 变化），chrome 在每次 `visibleChanged(true)` 时重新施加，重开控制台阴影丢失即此因。动态改 `flags`（如 StaysOnTop）后需 hide+show 才生效。

## 架构分层

- `src/scripting/api/`：脚本可见的 C++ 接口与数据模型（Qt-free）。ScriptConsole（环形缓冲日志）、ScriptEvents.h（事件结构体与取消注入）、ScriptManifest（描述文件解析校验）、ScriptFs/ScriptHttp（fs/http 域逻辑）、ScriptHostServices（宿主能力虚接口）。
- `src/scripting/binding/ScriptBindings.cc`：唯一绑定文件，纯声明 + 必要胶水。
- `src/scripting/engine/`：引擎生命周期与派发。ScriptEngineState（每脚本引擎状态）、ScriptEventBus（钩子派发）、ScriptingBootstrap（扫描加载编排）。
- `src/scripting/qt/ScriptQtHost`：`ScriptHostServices` 接口的 Qt 实现（QTimer 定时器/泵）。
- `src/qml/`：`ScriptLogModel`、`QmlScriptManager`（展示模型）。
- `src-common/CurlEx.{h,cc}`：通用 curl RAII 封装（namespace `edm::curl`）。downloader 侧经 `CurlFromContext.{h,cc}` 组装。
- 分层禁令：`src-downloader` 不含脚本/Qt；`src/scripting` 不含 Qt；Qt 仅在 `qt/`、`src/qml`、`AppService/EdmApplication`。

## 设计约束

- 宿主能力经 `ScriptHostServices` 虚接口注入（定时器/泵/托盘/配置/任务查询），实现类 `qt::QtHostServices`；新增宿主能力加虚方法 + Qt 实现 + handler 字段。
- jspp 内建能力优先：成员指针 `prop/prop_readonly`、typed `func/method`（参数返回值自动转换）、`.func` 重载列表、`magic_enum` 自动枚举、`inherit` 原型链、懒类绑定。仅绑定无法覆盖的逻辑手写胶水。
- 事件模型：`ScriptEvent` 基类 private 存取消状态与原因，`Cancellable<Derived>` CRTP 经 friend 注入 `cancel()/cancel(reason)/setCancelled(bool)/isCancelled()/cancelReason()`；绑定层用 `requires` 检测 cancel 能力决定是否注入命令式方法（`defEventClass<E>` 包装）。取消状态一律以方法暴露，不导出只读属性。取消原因经 `HookResult.reason` 传入任务终态错误信息。
- 元对象（ClassMeta/EnumMeta/ModuleMeta）必须与引擎同生命周期：一律 `edmModule()` 内函数级 static。jspp 按指针存储，传临时必崩。
- 脚本 API 走 static class 分组（events/logger/http/tasks/fs/path/utils/constants），notify 与 setTimeout/setInterval/clearTimeout/clearInterval 为模块顶层函数；禁顶层散函数堆积。
- 魔术值（kInvalidTaskID 等）经 `constants` 导出，脚本侧禁止硬编码。

## 禁止项

- 禁止 emoji 与装饰符号出现在代码、注释、UI；仅简中、ASCII 与 Fluent 图标字体。
- 注释与文档禁止引用作为设计参考的特定领域中小项目（如 LeviLamina、PLand）；大型基础设施
  项目与 cmake 拉取的依赖不受限（如 Node.js path.posix、jspp、Qt）。
- 禁止提交未经 clang-format 的 C++ 改动（仓库 .clang-format）。
- 禁止说教/立场/隐含上下文的注释；代码已表达的不注释；doxygen 单行风格。
- 禁止在绑定层手写 getter/setter 胶水（成员指针可覆盖）；禁止手撕格式化算法（用 fmt）。
- 禁止 `JSON.stringify` 慢路径取脚本值（用 `JS_JSONStringify` + `ValueHelper::unwrap`）。
- 禁止 BlockingQueuedConnection 跨脚本线程等待（退出死锁）。
- 禁止工作线程直接操作 jspp JobQueue（销毁竞态 UAF），经 `EngineOwnData::postTask` 托管队列。

## 文本与格式约定

- 注释只做第三人称技术描述：需求由来、修复历史、行为对比（"此前"、"本次"、"与历史一致"）一律不写；
  接到的要求不得转述为注释内容。
- 禁止说教、命令式语气、模糊指代与长篇论述；一句说清，能删则删。
- 文档按受众分离：docs/scripting.md 面向脚本作者，本交接单面向宿主维护者，实现细节不进用户文档。
- README 面向使用者介绍项目与功能，不穷举清单，以可读性为先。
- markdown 散文优先：表格、列表与嵌套仅用于真正结构化的数据，无必要不结构化；
  参照大型基础设施项目的文档写法。
- 脚本 API 以 docs/edm.d.ts 为唯一权威参考，其余文档不手抄 API 签名。
- 已删除或已废弃的内容不在文档中留痕。
- 全部文本不得互相矛盾；接口现状变更时同步所有引用处（README、d.ts、示例、注释）。
- 图标字形以 \uXXXX 转义书写并附字形名注释，不写入字面 PUA 字符。

## 偏好

- 命令式事件方法：`cancel()/cancel(reason)/cancelReason()/setCancelled(bool)/isCancelled()`，cancel 可选携带原因。
- options 参数用 plain object + TypeConverter 特化整体 toCpp。
- 存储统一走 fs 与脚本侧 JSON 序列化。
- 脚本入口 `export default class` + `onLoad/onEnable/onDisable`。

## 实现要点

- 加载流程：QML 就绪后扫描脚本目录（按目录名排序），逐个校验 manifest；每个启用脚本
  创建独立 jspp 引擎并注册原生模块，loadModule 后实例化 default export 类，依次调用
  `onLoad`/`onEnable`。任一步失败该脚本标记为错误并卸载，不影响其他脚本。
- 线程模型：脚本代码只在主线程执行。可变钩子由引擎后台线程经"主线程投递 + 条件变量
  等待（带超时）"派发，`hookTimeoutMs` 约束单钩子最长阻塞；通知钩子经 QueuedConnection 派发。
- 资源回收：引擎创建点以 `make_shared` 挂载 EngineOwnData（`Engine::setData`），jspp
  保证其析构先于 runtime 释放，监听器、定时器、挂起请求、长期句柄在析构中统一回收；
  长期持有的脚本回调使用 `TrackedGlobal`，引擎销毁自动置空。
- http 能力：响应体上限 16MB，超出报错；结果经 `JobQueue::postTask` 回注主线程泵，
  与钩子等待循环、低频泵（50ms）两种消费方式兼容。
- 持久化：可变钩子改写的 url/userAgent/fileName/headers/origin/referer/cookie 落库
  （`customHeaders` 为 JSON 对象列），断点续传与重试沿用；`proxyUrl` 为会话级不持久化。

## 踩坑记录

1. jspp `export_enum/export_class` 按指针存元：传临时（如 `export_enum(bindEnum<E>(...))`）会崩溃或悬垂，函数级 static 解决。
2. `inherit` 要求基类暴露给 JS 且 `hasConstructor()`；`ctor(nullptr)` 的类不能作基类。失败报 "Base class not registered: <派生类名>"（消息指向派生类，有误导）。
3. `BlockingQueuedConnection` 在应用退出时主线程停止调度，后台线程永久卡死在等待；改 Queued 投递加 condition_variable 超时等待。
4. http 工作线程回调不能经 Qt 队列（可变钩子 Promise 等待循环不处理 Qt 事件，resolve 永远送达）；改经 `EngineOwnData` 托管队列，两处泵消费。
5. Promise 等待期间不持有 EngineScope（跨线程 resolve 需要）；jspp::Exception 的 message/stacktrace 须在 EngineScope 存活期内消费，catch 放 scope 内。
6. MSVC CRT assert 默认弹模态对话框，卡死无人值守运行；测试入口 `_CrtSetReportMode` 重定向 stderr。
7. GUI 懒加载页（Loader）：启动后不导航则页面 QML 错误不暴露；验证时临时将 `App.qml` 默认页切到目标页，跑全量日志检查，验后还原。
8. GUI 链接失败 "permission denied" = exe 仍在运行，先 taskkill。
9. `TaskContext.url` 已去 const（beforeFetchMeta 改写需要）；DB 用 UPSERT 保 headers 行（INSERT OR REPLACE 会级联删 headers）。
10. bash heredoc 传 python 反斜杠转义不可靠（CRLF/转义丢失），多行文件修改用 Edit/perl -0777 或脚本文件。
11. `ScriptEngineState::services()` 返回裸指针（接口方法非 const），调用方需判 services_ 非空（initialize 前 QML 可能触发 discoverScripts）。
12. `qt_add_qml_module` 直挂 exe 且 Qt 动态链接时，生成的 `qml_register_types_edm_qml_ui()` 无人调用（无静态插件导入路径），QML_ELEMENT 类型（ScriptLogModel 等）在 QML 中全部不可见，报 "ReferenceError: XxxModel is not defined"。须在 `_initializeQml` 里加载 QML 前手动调用（EdmApplication.cc 顶部 extern 声明，置于全局作用域）。
13. 示例脚本 API 变更后，`%LOCALAPPDATA%/EasyDownloadManager/scripts` 里的旧副本不会自愈，GUI 报 "Could not find export 'listen' in module 'edm'"（QuickJS 模块实例化期错误），重新拷贝 `examples/scripts/*` 即可。
14. MCP/短超时构建被掐断会留僵尸 ninja 进程（锁 `.ninja_deps`，configure 报 "failed recompaction: Permission denied"）且可能杀断 configure 内的 vcpkg manifest install（`vcpkg_installed` 只剩 share/）。`taskkill //IM ninja.exe //F` 清理进程后，经 CLI `cmake --build` 长超时构建。
15. 自定义 QML role 返回 invalid QVariant 时 QML 侧得到 undefined，赋值报 "Unable to assign [undefined] to QString"。可选 string role（如 icon）缺值时必须返回空 `QString`，不是 `{}`。
16. `Q_INVOKABLE` 参数用指针类型（如 `QWindow*`）时头文件必须 include 完整类型（仅前向声明会触发 qmetatype 静态断言 "Pointer Meta Types must point to fully-defined types"）。
17. 绑定层 `requires` 探测失败是静默的：`Cancellable` 缺 `cancel(reason)` 重载时探测恒 false，`defEventClass` 整个分支被丢弃，脚本侧事件方法不存在且无编译错误。d.ts 声明与绑定导出须成对核对，cancelReason 方法与 cancel 可选参数曾同时缺失。
18. `_deps` 内的 FetchContent 副本被中断的构建/配置破坏后（如半克隆、.git 缺失），增量 configure 无法自愈；按既定流程删 `_deps` 重新配置，GitHub 拉取需走代理环境变量。

## 验证手段

- 构建：`cmake --build cmake-build-debug-clang-cl --target edm-scripting-smoke --target EasyDownloadManager`。
- E2E：启动 `test/server/server.js`（`TEST_FILE_SIZE=33554432 TEST_FILE_NAME=e2e-test.bin`，路由为 `/file` 与 `/file-range`），设 `EDM_SCRIPT_DIR=<repo>/.tmp/test-scripts`、`EDM_TEST_URL=http://127.0.0.1:3187/file`、`EDM_TEST_DIR` 后运行 `edm-scripting-smoke.exe`，退出码 0 = 全 PASS。`.tmp/test-scripts` 含 error-demo（onLoad 抛错验证隔离）与 async-rewriter（Promise 改写验证）。
- GUI：临时将 `App.qml` viewLoader 默认 source 切到 `ExtensionView.qml`，跑 GUI 后全量检查日志（不过滤），验后还原。
- 网络失败时用代理 `http://127.0.0.1:7890`。

## 未决项

- fs 无沙箱：绝对路径可达任意位置。
- 事件 `proxyUrl` 改写为会话级，不持久化。
- 引擎卸载竞态时挂起的 http 任务随队列销毁（单任务指针量级泄漏，仅该路径）。
- jspp 跟踪 main 分支（`FETCHCONTENT_UPDATES_DISCONNECTED=ON`），更新需删 `_deps` 重新拉取。
