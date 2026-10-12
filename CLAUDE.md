# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

A Qt6/C++23 download manager. One top-level CMake defines three build targets:

- **`src-downloader`** (`src-downloader/`) — Qt-free static library containing all download engine logic (curl, segmented multi-range downloads, resume, persistence). Namespaces `edm` and `edm::downloader`.
- **`src-downloader-cli`** (`test/cli/cli_main.cc`) — manual CLI harness that runs a single download through the engine. Built by default.
- **`EasyDownloadManager`** (`src/`) — Qt6 application (QML UI with a thin Widgets shim) that links `src-downloader`. Owns the dispatcher, database, config, tray icon, and QML bridge layer.

`src-ui/` holds the QML UI sources (Qt Design Studio project) and is pulled in by `qt_add_qml_module` in the build. `test/` holds the manual CLI harness and a small Node HTTP test server.

Dependencies are managed with a single **vcpkg manifest** (`vcpkg.json` at the repo root): curl (http2+ssl), SQLiteCpp, fmt, spdlog, glaze, magic-enum. Qt6 (Widgets + Quick + QuickControls2 + QuickLayouts + QuickDialogs2) is supplied via `CMAKE_PREFIX_PATH`, not vcpkg.

## Build & configure

CMake, C++23, MSVC on Windows. `.vscode/settings.json` pins the toolchain — mirror these args when configuring elsewhere:

```
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_PREFIX_PATH=<Qt>/6.8.3/msvc2022_64
```

The MSVC runtime is forced to the **DLL** debug/release variant (`MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`) on all targets — vcpkg binaries must match.

`WIN32_EXECUTABLE` is `FALSE` so `qDebug` output is visible in a console. `EDM_DEBUG` is defined in Debug builds. `SPDLOG_FMT_EXTERNAL`, `CURL_STATICLIB`, and `NOMINMAX` are defined on the `src-downloader` target.

## Running tests / CLI

There is no unit-test framework. The only executable target is **`src-downloader-cli`** (`test/cli/cli_main.cc`), a manual harness that runs a single download via the `Dispatcher` and prints `task_state=`/`downloaded_bytes=`/etc. It is exercised against `http://127.0.0.1:3187/file`, served by the Node server in `test/server/` (`node test/server/server.js`).

```
src-downloader-cli --url <url> --output-dir <dir> [--threads N] [--retry N] [--timeout-seconds N] [--user-agent X] [--proxy url]
```

## Architecture

### Download engine (`src-downloader`)

Layered, callback-driven, thread-per-range:

- **`Dispatcher`** (`Dispatcher.h`) — top-level orchestrator. Holds `activeTasks_` (id → `DownloadTask`) under a mutex. Constructed with three callbacks injected by the host: `taskLoader` (fetch persisted config by id), `onTaskChanged` (persist state changes), `configureFactory` (build `TaskContext` from model). Entry points: `createTask`, `pauseTask`, `resumeTask`, `cancelTask`, `updateSpeed`/`updateAllSpeeds`. New tasks without a real DB id get a transient id from `nextTransientId_`.
- **`downloader::DownloadTask`** — owns the live task: spawns N `std::thread` workers (`launchWorkers`/`workerLoop`), tracks per-range progress in `DownloadState`, handles pause/resume/cancel via an `isRunningFlag_` atomic shared with workers, persists progress to a `.meta` sidecar (`loadProgress`/`saveProgress`) for resume, and notifies the host through `StateChangedCallback`. Non-copyable, non-movable (`enable_shared_from_this`).
- **`downloader::DownloadWorker`** — one curl transfer per range (`downloadRange`) or whole-file (`downloadWholeFile` when server doesn't support ranges). Receives pause/cancel through the shared `isTaskRunning_` atomic.
- **`DownloadState`** — atomics for state/bytes/speed, `shared_mutex`-guarded strings (error, output path), and the `ranges` vector. This is what the GUI polls for live updates.
- **`TaskContext`** (`DownloadTypes.h`) — curl session factory (`newCurl()`), built from a `TaskModel`. Carries proxy, UA, headers (origin/referer/cookie), thread/bandwidth/retry.
- **`Global.h`** — defines the value types (`TaskId`, `FileSize`, `BandLimit`), enums (`TaskState`, `Category`, `Resumable`, `ProxyType`), and `kInvalid*` sentinels.
- **`expected.h`** — `Expected<T>` = `nonstd::expected<T, edm::Error>`. `Error` is move-only, holds an `IErrorInfo`. Use `makeStringError(...)` / `makeError<T>(...)` to produce `Unexpected`, `forwardError(err)` to rethrow up the stack. **Do not throw** — propagate via `Expected`.
- **`TaskLogger`** — per-task spdlog logger keyed by `TaskId`; always use the `EDM_LOG_*(taskId, fmt, ...)` macros, never `spdlog::` directly. `SPDLOG_FMT_EXTERNAL` is defined (uses the vcpkg `fmt`).
- **`schema::ChunkMetaFile`** (`schema/ChunkMetaFile.h`) — glaze-based BEVE binary serialization for `.meta` sidecar files. `Envelope<T>` wraps a payload with magic/version/checksum; `EnvelopeWriter`/`EnvelopeReader` handle I/O.

### GUI host (`src/`)

The UI is **Qt Quick / QML**, not Widgets. Widgets are only used for `QApplication`, `QSystemTrayIcon`, and `QMenu` — all visible UI is QML.

**QML bridge layer** (`src/qml/`) — C++ objects exposed to QML as singletons:

- **`QmlTaskListModel`** — the central bridge. `QAbstractListModel` backed by `Dispatcher` + `DownloadDatabase`. Exposes live task rows to QML's `ListView`. Has a 1-second QTimer that calls `Dispatcher::updateAllSpeeds()` and emits `dataChanged`. Entry points for all task actions: `createTask(url, saveDir, useProxy)`, `pauseTask(id)`, `resumeTask(id)`, `cancelTask(id)`, `deleteTask(id)`. `reloadFromDatabase()` restores persisted tasks on startup. `getTaskDetail(id)` returns a lazily-created `QmlTaskDetail`.
- **`QmlTaskDetail`** — QML-uncreatable per-task detail object. Exposes live speed, ETA, error string, and a `rangesModel` property (a `QmlRangeModel`) exposing per-thread download progress as a list model.
- **`QmlSettings`** — wraps `EdmConfig` singleton with `Q_PROPERTY`/NOTIFY signals. QML two-way binds settings widgets to persistent config (threads, bandwidth, UA, proxy, save dir, autostart, notifications).
- **`QmlController`** — app-level actions: minimize to tray, open file/folder, clipboard auto-paste.

**Core application wiring:**

- **`EdmApplication`** — singleton (constructed after `QApplication`) that wires everything together in `initApp()`: creates `DownloadDatabase`, `Dispatcher` (injecting DB-backed loader/persister callbacks), `QQmlApplicationEngine`, registers `TaskListModel`/`SettingsModel`/`AppController` as QML singletons, loads `App.qml`, starts the refresh timer, and creates the system tray icon.
- **`DownloadDatabase`** — SQLiteCpp-backed persistence (`insertTask`, `getTaskById`, `forEachTask`, ...), mutex-guarded. Two tables: `downloads` and `headers`. Path from `EdmConfig::getDatabasePath()`.
- **`TaskModel` / TaskModelConversion** (`src/database/`) — `TaskModel` is the DB-facing data struct. `toTaskContext()` converts `TaskModel` → engine `TaskContext`; `applyContextToModel()` does the reverse.
- **`EdmConfig`** — singleton over `QSettings` for global settings (threads, bandwidth, UA, `ProxyConfig`, save dir, autostart, completion dialog). `ProxyConfig::toProxyUrl()` serializes to curl's `[proto://][user[:pass]@]host[:port]` form.
- **`EventBus`** — singleton `QObject` with Qt signals (`onRequestCreateTask`, `onTaskCreated`). **Legacy** — the primary path no longer goes through it; QML calls `QmlTaskListModel::createTask()` directly. **`SignalHandler`** bridges `EventBus::onRequestCreateTask` → `QmlTaskListModel::createTask()` as a fallback for non-QML callers.

### Data flow

**Primary path (QML):**
`NewTaskDialog.qml` → `TaskListModel.createTask(url, saveDir, useProxy)` → constructs `TaskModel`, inserts into DB for a valid `TaskId`, converts to `TaskContext` via `toTaskContext()`, calls `Dispatcher::createTask()` → `DownloadTask` starts workers → 1s timer calls `Dispatcher::updateAllSpeeds()` → `dataChanged` signal → QML `ListView` rebinds.

**Fallback path (EventBus):**
`EventBus::onRequestCreateTask` → `SignalHandler` → delegates to `QmlTaskListModel::createTask()` (or calls `Dispatcher` directly as last resort).

**Persistence path:**
Worker progress → `DownloadTask::saveProgress()` → `.meta` sidecar. Dispatcher's `onTaskChanged` callback → `applyContextToModel()` + `database_->insertTask()` → SQLite.

## Conventions

- C++23. Format with the repo `.clang-format` (LLVM-based, 4-space access offset, block-indent open brackets). clangd reads `compile_commands.json` from `build/` — generate it (`CMAKE_EXPORT_COMPILE_COMMANDS`) for IDE support.
- `.clang-tidy` is configured (bugprone/cert/modernize/performance checks); don't introduce new warnings.
- Comments and identifiers are largely in **Chinese** (struct field comments, README). Match the surrounding file's language when editing.
- Error handling is `Expected`-based throughout the engine (`src-downloader`); never let exceptions cross into the engine.
- Keep `src-downloader` Qt-free: new engine code goes in `src-downloader/` and must not include `Q*` headers.
- Bridge objects (`Qml*`) live in `src/qml/` and may use Qt freely. They are registered as QML singletons in `EdmApplication::initApp()`.
- QML sources live in `src-ui/`; do not edit QML files in `build/` — they are copied artifacts.
- The QML style is "Basic" (`QQuickStyle::setStyle("Basic")`). Custom FluentUI components are in `src-ui/Src/fluent/`.
