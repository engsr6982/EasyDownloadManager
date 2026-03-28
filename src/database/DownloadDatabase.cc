#include "DownloadDatabase.h"

#include "config/EdmGlobalConfig.h"
#include "model/TaskModel.h"
#include "utils/StringUtils.h"

#include <random>

namespace edm {


DownloadDatabase::DownloadDatabase() {
    db_ = std::make_unique<SQLite::Database>(
        string_utils::qstring2string(EdmGlobalConfig::getDatabasePath()),
        SQLite::OPEN_CREATE | SQLite::OPEN_READWRITE
    );
    initTables();
}

void DownloadDatabase::initTables() const {
    // downloads 表
    {
        SQLite::Statement query(
            *db_,
            R"(
                CREATE TABLE IF NOT EXISTS downloads (
                    id INTEGER PRIMARY KEY,
                    url TEXT NOT NULL,
                    method INTEGER NOT NULL,         -- RequestType 枚举
                    fileName TEXT NOT NULL,
                    fileSize INTEGER NOT NULL,       -- int64
                    category INTEGER NOT NULL,       -- Category 枚举
                    state INTEGER NOT NULL,          -- TaskState 枚举
                    bandWidthLimit INTEGER NOT NULL,
                    threadCount INTEGER NOT NULL,
                    firstTry INTEGER NOT NULL,       -- int64 时间戳
                    lastTry INTEGER NOT NULL,        -- int64 时间戳
                    userAgent TEXT NOT NULL,
                    resumable INTEGER NOT NULL,      -- Resumable 枚举
                    pageUrl TEXT NOT NULL,
                    pageTitle TEXT NOT NULL,
                    mimeType TEXT NOT NULL,
                    errorMsg TEXT NOT NULL,
                    postBody TEXT NOT NULL,
                    saveDir TEXT NOT NULL,
                    tempDir TEXT NOT NULL
                );
            )"
        );
        query.exec();
    }

    // headers 表
    {
        SQLite::Statement query(
            *db_,
            R"(
                CREATE TABLE IF NOT EXISTS headers (
                    id INTEGER PRIMARY KEY,   -- 关联 downloads.id
                    origin TEXT,
                    cookie TEXT,
                    referer TEXT,
                    FOREIGN KEY (id) REFERENCES downloads(id) ON DELETE CASCADE
                );
            )"
        );
        query.exec();
    }
}

void DownloadDatabase::insertTask(TaskModel& task) {
    std::lock_guard   lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            INSERT OR REPLACE INTO downloads (
                id,
                url,
                method,
                fileName,
                fileSize,
                category,
                state,
                bandWidthLimit,
                threadCount,
                firstTry,
                lastTry,
                userAgent,
                resumable,
                pageUrl,
                pageTitle,
                mimeType,
                errorMsg,
                postBody,
                saveDir,
                tempDir
            ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
        )"
    );

    int idx = 1;
    if (task.id > 0) {
        query.bind(idx++, task.id);
    } else {
        query.bind(idx++, nullptr); // 让 SQLite 自动分配 id
    }

    query.bind(idx++, task.url);
    query.bind(idx++, static_cast<int>(task.method));
    query.bind(idx++, task.fileName);
    query.bind(idx++, task.fileSize);
    query.bind(idx++, static_cast<int>(task.category));
    query.bind(idx++, static_cast<int>(task.state));
    query.bind(idx++, task.bandWidthLimit);
    query.bind(idx++, task.threadCount);
    query.bind(idx++, task.firstTry);
    query.bind(idx++, task.lastTry);
    query.bind(idx++, task.userAgent);
    query.bind(idx++, static_cast<int>(task.resumable));
    query.bind(idx++, task.pageUrl);
    query.bind(idx++, task.pageTitle);
    query.bind(idx++, task.mimeType);
    query.bind(idx++, task.errorMsg);
    query.bind(idx++, task.postBody);
    query.bind(idx++, task.saveDir);
    query.bind(idx++, task.tempDir);

    query.exec();

    // 插入后更新 task.id
    if (task.id <= 0) {
        task.id = static_cast<int>(db_->getLastInsertRowid());
    }
}

void DownloadDatabase::insertTaskHeader(TaskHeaderModel const& taskHeader) {
    std::lock_guard lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            INSERT OR REPLACE INTO headers (
                id,
                origin,
                cookie,
                referer
            ) VALUES (?,?,?,?)
        )"
    );

    query.bind(1, taskHeader.id);
    query.bind(2, taskHeader.origin);
    query.bind(3, taskHeader.cookie);
    query.bind(4, taskHeader.referer);

    query.exec();
}

std::optional<TaskModel> DownloadDatabase::getTaskById(int id) const {
    std::lock_guard lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            SELECT
                id,
                url,
                method,
                fileName,
                fileSize,
                category,
                state,
                bandWidthLimit,
                threadCount,
                firstTry,
                lastTry,
                userAgent,
                resumable,
                pageUrl,
                pageTitle,
                mimeType,
                errorMsg,
                postBody,
                saveDir,
                tempDir
            FROM downloads
            WHERE id=?
        )"
    );

    query.bind(1, id);

    if (!query.executeStep()) {
        return std::nullopt;
    }

    TaskModel task;
    int       idx       = 0;
    task.id             = query.getColumn(idx++).getInt();
    task.url            = query.getColumn(idx++).getString();
    task.method         = static_cast<RequestType>(query.getColumn(idx++).getInt());
    task.fileName       = query.getColumn(idx++).getString();
    task.fileSize       = query.getColumn(idx++).getInt64();
    task.category       = static_cast<Category>(query.getColumn(idx++).getInt());
    task.state          = static_cast<TaskState>(query.getColumn(idx++).getInt());
    task.bandWidthLimit = query.getColumn(idx++).getInt();
    task.threadCount    = query.getColumn(idx++).getInt();
    task.firstTry       = query.getColumn(idx++).getInt64();
    task.lastTry        = query.getColumn(idx++).getInt64();
    task.userAgent      = query.getColumn(idx++).getString();
    task.resumable      = static_cast<Resumable>(query.getColumn(idx++).getInt());
    task.pageUrl        = query.getColumn(idx++).getString();
    task.pageTitle      = query.getColumn(idx++).getString();
    task.mimeType       = query.getColumn(idx++).getString();
    task.errorMsg       = query.getColumn(idx++).getString();
    task.postBody       = query.getColumn(idx++).getString();
    task.saveDir        = query.getColumn(idx++).getString();
    task.tempDir        = query.getColumn(idx++).getString();

    return task;
}

std::optional<TaskHeaderModel> DownloadDatabase::getTaskHeaderById(int id) const {
    std::lock_guard lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            SELECT
                id,
                origin,
                cookie,
                referer
            FROM headers
            WHERE id=?
        )"
    );

    query.bind(1, id);

    if (!query.executeStep()) {
        return std::nullopt;
    }

    TaskHeaderModel taskHeader;
    int             idx = 0;
    taskHeader.id       = query.getColumn(idx++).getInt();
    taskHeader.origin   = query.getColumn(idx++).getString();
    taskHeader.cookie   = query.getColumn(idx++).getString();
    taskHeader.referer  = query.getColumn(idx++).getString();
    return taskHeader;
}

void DownloadDatabase::forEachTask(std::function<bool(TaskModel const&)> const& callback) const {
    std::lock_guard lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            SELECT
                id,
                url,
                method,
                fileName,
                fileSize,
                category,
                state,
                bandWidthLimit,
                threadCount,
                firstTry,
                lastTry,
                userAgent,
                resumable,
                pageUrl,
                pageTitle,
                mimeType,
                errorMsg,
                postBody,
                saveDir,
                tempDir
            FROM downloads
        )"
    );

    TaskModel task;
    int       idx{0};
    while (query.executeStep()) {
        idx                 = 0;
        task.id             = query.getColumn(idx++).getInt();
        task.url            = query.getColumn(idx++).getString();
        task.method         = static_cast<RequestType>(query.getColumn(idx++).getInt());
        task.fileName       = query.getColumn(idx++).getString();
        task.fileSize       = query.getColumn(idx++).getInt64();
        task.category       = static_cast<Category>(query.getColumn(idx++).getInt());
        task.state          = static_cast<TaskState>(query.getColumn(idx++).getInt());
        task.bandWidthLimit = query.getColumn(idx++).getInt();
        task.threadCount    = query.getColumn(idx++).getInt();
        task.firstTry       = query.getColumn(idx++).getInt64();
        task.lastTry        = query.getColumn(idx++).getInt64();
        task.userAgent      = query.getColumn(idx++).getString();
        task.resumable      = static_cast<Resumable>(query.getColumn(idx++).getInt());
        task.pageUrl        = query.getColumn(idx++).getString();
        task.pageTitle      = query.getColumn(idx++).getString();
        task.mimeType       = query.getColumn(idx++).getString();
        task.errorMsg       = query.getColumn(idx++).getString();
        task.postBody       = query.getColumn(idx++).getString();
        task.saveDir        = query.getColumn(idx++).getString();
        task.tempDir        = query.getColumn(idx++).getString();

        // 如果回调返回 false，则提前停止遍历
        if (!callback(task)) {
            break;
        }
    }
}

#ifdef EDM_DEBUG
void DownloadDatabase::insertFakeTasks(int count) {
    if (count <= 0) count = 150;

    std::mt19937 rng(static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int64_t> sizeDist(1024, 1024 * 1024 * 500); // 1KB ~ 500MB
    std::uniform_int_distribution<int>     threadsDist(1, 8);

    for (int i = 0; i < count; ++i) {
        TaskModel task;
        task.id             = 0; // 自动分配
        task.url            = std::format("https://example.com/file{}.dat", i);
        task.method         = RequestType::GET;
        task.fileName       = std::format("file{}.dat", i);
        task.fileSize       = sizeDist(rng);
        task.category       = Category::Other;
        task.state          = TaskState::Finished;
        task.bandWidthLimit = 0;
        task.threadCount    = threadsDist(rng);
        task.firstTry       = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        task.lastTry        = task.firstTry;
        task.userAgent      = "FakeUserAgent/1.0";
        task.resumable      = Resumable::Yes;
        task.pageUrl        = "https://example.com";
        task.pageTitle      = "Example Page";
        task.mimeType       = "application/octet-stream";
        task.errorMsg       = "";
        task.postBody       = "";
        task.saveDir        = string_utils::qstring2string(EdmGlobalConfig::instance().getSaveDir());
        task.tempDir        = string_utils::qstring2string(EdmGlobalConfig::instance().getTempDir());

        insertTask(task);
    }
}
#endif

} // namespace edm