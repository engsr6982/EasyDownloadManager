/**
 * 下载引擎核心测试（无 GUI，直连 Dispatcher + test/server 测试服务器）。
 *
 * 先启动 test/server/server.js（含 /stats 连接计数与 Content-MD5 下发），
 * 再运行本测试。覆盖：
 *   S1 多线程下载：MD5 校验（引擎内建 + FileVerifier 独立复检）+ 连接复用统计
 *   S2 暂停/恢复循环：线程零重建（workerLaunchCount == 1）+ 幂等 + MD5
 *   S3 取消后同实例重启（worker reinit）：launchCount == 2 + MD5
 *   S4 并发任务：3 任务并行全数完成 + MD5 + 连接数受控
 *   S5 连接释放：暂停态与完成后服务端活跃连接归零（无 idle TCP 长挂）
 *   S6 临时文件命名与归属：并发同名任务临时文件互不误占，完成后按 name(1).ext 落地
 *
 * 环境：EDM_ENGINE_URL（默认 http://127.0.0.1:3187/file）
 * 输出：逐项 PASS/FAIL 汇总，退出码 0 = 全部通过。
 */

#include "Dispatcher.h"
#include "Global.h"
#include "ITaskObserver.h"
#include "downloader/DownloadTask.h"
#include "downloader/DownloadTypes.h"
#include "downloader/FileVerifier.h"
#include "schema/ChunkMetaFile.h"
#include "utils/TaskLogger.h"

#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr auto kServerBase = "http://127.0.0.1:3187";
constexpr auto kFileUrl    = "http://127.0.0.1:3187/file";

int                    failures{0};
std::atomic<long long> taskIdAlloc{0};

void check(bool ok, std::string const& what) {
    std::printf("%-4s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

std::string httpGet(std::string const& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
            return size * nmemb;
        }
    );
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return body;
}

/// 从固定结构的 JSON 里提取数值/字符串字段（测试专用，避免引入解析器依赖）
long long jsonInt(std::string const& json, std::string const& key) {
    auto const needle = "\"" + key + "\":";
    auto const pos    = json.find(needle);
    if (pos == std::string::npos) return -1;
    auto const valuePos = pos + needle.size();
    auto const end      = json.find_first_not_of("0123456789", valuePos);
    return std::stoll(json.substr(valuePos, end - valuePos));
}

std::string jsonStr(std::string const& json, std::string const& key) {
    auto const needle = "\"" + key + "\":\"";
    auto const pos    = json.find(needle);
    if (pos == std::string::npos) return {};
    auto const valuePos = pos + needle.size();
    auto const end      = json.find('"', valuePos);
    return json.substr(valuePos, end - valuePos);
}

struct TestObserver final : edm::ITaskObserver {
    void onStateChanged(std::shared_ptr<edm::TaskContext>& /*ctx*/, edm::TaskState /*old*/) override {}
    void onError(std::shared_ptr<edm::TaskContext>& /*ctx*/, std::string const& /*error*/) override {}
    void onFileNameChanged(std::shared_ptr<edm::TaskContext>& ctx) override {
        if (!ctx) return;
        std::lock_guard lock{mutex_};
        lastFileName_[ctx->id] = ctx->fileName;
    }

    /// 取某任务最近一次 onFileNameChanged 上报的文件名
    std::string fileNameOf(edm::TaskId id) const {
        std::lock_guard lock{mutex_};
        auto const      it = lastFileName_.find(id);
        return it == lastFileName_.end() ? std::string{} : it->second;
    }

    mutable std::mutex                 mutex_;
    std::map<edm::TaskId, std::string> lastFileName_;
};

long long connectionCount() { return jsonInt(httpGet(std::string(kServerBase) + "/stats"), "totalConnections"); }

long long activeConnectionCount() { return jsonInt(httpGet(std::string(kServerBase) + "/stats"), "activeConnections"); }

/// 轮询等待服务端活跃连接归零（断连经 FIN 到达服务端需要短暂时间）
bool waitNoActiveConnections(int timeoutMs = 5000) {
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (activeConnectionCount() == 0) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

std::string groundTruthMd5() { return jsonStr(httpGet(std::string(kServerBase) + "/checksum"), "fullFileMd5"); }

std::shared_ptr<edm::TaskContext> makeTask(edm::TaskId id, std::string const& saveDir, int threads) {
    auto ctx     = std::make_shared<edm::TaskContext>(id, kFileUrl, saveDir);
    ctx->threads = static_cast<edm::AvailableThreads>(threads);
    return ctx;
}

bool waitTerminal(edm::Dispatcher& dispatcher, edm::TaskId id, int timeoutSeconds) {
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
        dispatcher.updateSpeed(id);
        if (auto state = dispatcher.getTaskState(id)) {
            auto const s = state->state();
            if (s == edm::TaskState::Finished || s == edm::TaskState::Failed || s == edm::TaskState::Canceled) {
                return s == edm::TaskState::Finished;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::printf("      TIMEOUT waiting for task %lld terminal state\n", static_cast<long long>(id));
    return false;
}

bool waitForProgress(edm::Dispatcher& dispatcher, edm::TaskId id, double fraction, int timeoutSeconds) {
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
        dispatcher.updateSpeed(id);
        if (auto state = dispatcher.getTaskState(id)) {
            auto const total = state->totalBytes();
            if (total > 0) {
                double const f = static_cast<double>(state->downloadedBytes()) / static_cast<double>(total);
                if (f >= fraction) return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::printf("      TIMEOUT waiting for task %lld progress %.0f%%\n", static_cast<long long>(id), fraction * 100);
    return false;
}

bool verifyFile(std::string const& path, std::string const& expectedMd5) {
    auto const actual = edm::downloader::FileVerifier::md5Hex(path);
    if (!actual) {
        std::printf("      md5Hex failed: %s\n", actual.error().message().c_str());
        return false;
    }
    return *actual == expectedMd5;
}

} // namespace

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0); // 崩溃时日志不丢
    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::filesystem::create_directories(".tmp/engine");
    std::filesystem::create_directories(".tmp/logs/engine");
    edm::TaskLoggerManager::init(".tmp/logs/engine");

    auto const md5 = groundTruthMd5();
    std::printf("ground truth md5: %s\n", md5.c_str());

    TestObserver    observer;
    edm::Dispatcher dispatcher(&observer);

    std::printf("== S1 basic multi-worker download ==\n");
    {
        auto const id     = static_cast<edm::TaskId>(taskIdAlloc.fetch_add(1) + 1);
        auto const before = connectionCount();
        std::filesystem::create_directories(".tmp/engine/s1");

        check(static_cast<bool>(dispatcher.createTask(makeTask(id, ".tmp/engine/s1", 8))), "S1 createTask");
        bool const finished = waitTerminal(dispatcher, id, 180);
        check(finished, "S1 task finished (engine internal MD5 + size verification passed)");

        auto const after  = connectionCount();
        auto const delta  = after - before;
        bool const connOk = delta > 0 && delta <= 8 + 8;
        std::printf("      connections used: %lld (workers=8, chunks~32)\n", delta);
        check(connOk, "S1 connection reuse: delta within workers+slack (would be ~33 without reuse)");

        bool const md5Ok = verifyFile(".tmp/engine/s1/e2e-test.bin", md5);
        check(md5Ok, "S1 file MD5 matches server ground truth");
        bool const noTemp = !std::filesystem::exists(".tmp/engine/s1/e2e-test.bin.edm")
                         && !std::filesystem::exists(".tmp/engine/s1/e2e-test.bin.edm.meta");
        check(noTemp, "S1 no .edm/.meta leftover after finish");
        if (!(finished && connOk && md5Ok && noTemp)) ++failures;
    }

    std::printf("== S2 pause/resume cycles (thread reuse) ==\n");
    {
        auto const id = static_cast<edm::TaskId>(taskIdAlloc.fetch_add(1) + 1);
        std::filesystem::create_directories(".tmp/engine/s2");
        check(static_cast<bool>(dispatcher.createTask(makeTask(id, ".tmp/engine/s2", 4))), "S2 createTask");

        bool                      allOk       = true;
        std::vector<double> const checkpoints = {0.15, 0.45, 0.75};
        for (double cp : checkpoints) {
            if (!waitForProgress(dispatcher, id, cp, 120)) {
                allOk = false;
                break;
            }
            auto const paused = dispatcher.pauseTask(id);
            check(
                static_cast<bool>(paused),
                ("S2 pause at " + std::to_string(static_cast<int>(cp * 100)) + "%").c_str()
            );
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto state = dispatcher.getTaskState(id);
            check(state && state->state() == edm::TaskState::Paused, "S2 state Paused");
            auto const resumed = dispatcher.resumeTask(id);
            check(static_cast<bool>(resumed), "S2 resume");
        }

        bool const finished = allOk && waitTerminal(dispatcher, id, 180);
        check(finished, "S2 task finished after pause/resume cycles");

        auto const task = dispatcher.getActiveTask(id);
        check(task && task->workerLaunchCount() == 1, "S2 worker threads reused (single spawn across all resumes)");

        bool const md5Ok = verifyFile(".tmp/engine/s2/e2e-test.bin", md5);
        check(md5Ok, "S2 file MD5 matches ground truth (idempotency across pause/resume)");
        if (!(allOk && finished && task && task->workerLaunchCount() == 1 && md5Ok)) ++failures;
    }

    std::printf("== S3 cancel + same-instance restart (worker reinit) ==\n");
    {
        auto const id = static_cast<edm::TaskId>(taskIdAlloc.fetch_add(1) + 1);
        std::filesystem::create_directories(".tmp/engine/s3");
        check(static_cast<bool>(dispatcher.createTask(makeTask(id, ".tmp/engine/s3", 4))), "S3 createTask");
        bool ok = waitForProgress(dispatcher, id, 0.10, 120);

        auto const canceled = ok ? dispatcher.cancelTask(id) : edm::Expected<>{};
        check(ok && static_cast<bool>(canceled), "S3 cancel mid-download");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto state = dispatcher.getTaskState(id);
        check(state && state->state() == edm::TaskState::Canceled, "S3 state Canceled");

        auto const restarted = dispatcher.resumeTask(id);
        check(static_cast<bool>(restarted), "S3 restart after cancel (same instance reinit)");
        bool const finished = waitTerminal(dispatcher, id, 180);
        check(finished, "S3 task finished after restart");

        auto const task     = dispatcher.getActiveTask(id);
        auto const launches = task ? task->workerLaunchCount() : 0;
        check(launches == 2, "S3 worker reinit: exactly 2 spawn batches (cancel exit + restart)");

        bool const md5Ok = verifyFile(".tmp/engine/s3/e2e-test.bin", md5);
        check(md5Ok, "S3 file MD5 matches ground truth after cancel/restart");
        bool const noTemp = !std::filesystem::exists(".tmp/engine/s3/e2e-test.bin.edm")
                         && !std::filesystem::exists(".tmp/engine/s3/e2e-test.bin.edm.meta");
        check(noTemp, "S3 no .edm/.meta leftover after finish");
        if (!(ok && static_cast<bool>(canceled) && static_cast<bool>(restarted) && finished && launches == 2 && md5Ok))
            ++failures;
    }

    std::printf("== S4 concurrent tasks ==\n");
    {
        std::vector<std::string> const dirs = {".tmp/engine/s4a", ".tmp/engine/s4b", ".tmp/engine/s4c"};
        std::vector<edm::TaskId>       ids;
        auto const                     before = connectionCount();

        for (auto const& d : dirs) {
            std::filesystem::create_directories(d);
            auto const id = static_cast<edm::TaskId>(taskIdAlloc.fetch_add(1) + 1);
            ids.push_back(id);
            check(static_cast<bool>(dispatcher.createTask(makeTask(id, d, 4))), "S4 createTask (concurrent)");
        }

        bool allFinished = waitTerminal(dispatcher, ids.back(), 240);
        for (auto const id : ids) {
            auto state = dispatcher.getTaskState(id);
            if (!state || state->state() != edm::TaskState::Finished) allFinished = false;
        }
        check(allFinished, "S4 all concurrent tasks finished");

        auto const after = connectionCount();
        auto const delta = after - before;
        std::printf("      connections used: %lld (3 tasks x 4 workers)\n", delta);
        check(delta > 0 && delta <= 3 * 4 + 12, "S4 connection reuse under concurrency");

        bool md5Ok = true;
        for (auto const& d : dirs) {
            if (!verifyFile(d + "/e2e-test.bin", md5)) {
                md5Ok = false;
                std::printf("      MD5 mismatch in %s\n", d.c_str());
            }
        }
        check(md5Ok, "S4 all files MD5 match ground truth");
        if (!(allFinished && md5Ok)) ++failures;
    }

    std::printf("== S5 connection release (no idle TCP) ==\n");
    {
        auto const id = static_cast<edm::TaskId>(taskIdAlloc.fetch_add(1) + 1);
        std::filesystem::create_directories(".tmp/engine/s5");
        check(static_cast<bool>(dispatcher.createTask(makeTask(id, ".tmp/engine/s5", 4))), "S5 createTask");
        check(waitForProgress(dispatcher, id, 0.15, 120), "S5 progress reached 15%");

        auto const paused = dispatcher.pauseTask(id);
        check(static_cast<bool>(paused), "S5 pause mid-download");
        bool const pausedReleased = waitNoActiveConnections();
        std::printf("      active connections while paused: %lld\n", activeConnectionCount());
        check(pausedReleased, "S5 paused task holds no server connection");

        check(static_cast<bool>(dispatcher.resumeTask(id)), "S5 resume");
        bool const finished = waitTerminal(dispatcher, id, 180);
        check(finished, "S5 task finished");
        bool const finishedReleased = waitNoActiveConnections();
        std::printf("      active connections after finish: %lld\n", activeConnectionCount());
        check(finishedReleased, "S5 finished task holds no server connection");

        bool const md5Ok = verifyFile(".tmp/engine/s5/e2e-test.bin", md5);
        check(md5Ok, "S5 file MD5 matches ground truth");
        if (!(pausedReleased && finished && finishedReleased && md5Ok)) ++failures;
    }

    std::printf("== S6 temp file naming & ownership ==\n");
    {
        namespace fs = std::filesystem;
        fs::create_directories(".tmp/engine/s6");
        fs::remove_all(".tmp/engine/s6/content"); // 复跑时清场
        fs::create_directories(".tmp/engine/s6/content");

        // 两个任务下同名文件到同一目录：临时文件按 Windows 计数规则各自唯一，互不误占
        auto const idA = static_cast<edm::TaskId>(taskIdAlloc.fetch_add(1) + 1);
        auto const idB = static_cast<edm::TaskId>(taskIdAlloc.fetch_add(1) + 1);
        check(static_cast<bool>(dispatcher.createTask(makeTask(idA, ".tmp/engine/s6/content", 4))), "S6 createTask A");
        check(static_cast<bool>(dispatcher.createTask(makeTask(idB, ".tmp/engine/s6/content", 4))), "S6 createTask B");

        // 等到两个任务各完成至少一个分片（.meta 已落盘）再暂停，避免暂停过早无归属信息
        bool const progressed =
            waitForProgress(dispatcher, idA, 0.15, 120) && waitForProgress(dispatcher, idB, 0.15, 120);
        check(progressed, "S6 both tasks wrote progress before pause");

        bool const pausedA = static_cast<bool>(dispatcher.pauseTask(idA));
        bool const pausedB = static_cast<bool>(dispatcher.pauseTask(idB));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 暂停态：恰好两个 .edm 临时文件，且 .meta 归属各自的 taskId
        std::vector<fs::path> tempFiles;
        for (auto const& entry : fs::directory_iterator(".tmp/engine/s6/content")) {
            if (entry.path().extension() == ".edm") tempFiles.push_back(entry.path());
        }
        bool const namingOk = tempFiles.size() == 2;
        check(namingOk, "S6 two distinct temp files for two same-name tasks");
        for (auto const& t : tempFiles) std::printf("      temp: %s\n", t.filename().string().c_str());

        auto const metaOwner = [&](fs::path const& temp) -> edm::TaskId {
            auto reader = edm::schema::EnvelopeReader{
                edm::schema::kProgressMagic,
                edm::schema::kProgressVersion,
                temp.string() + ".meta"
            };
            auto data = reader.read<edm::schema::MetaPaylad>();
            return data ? data->taskId : -1;
        };
        std::vector<edm::TaskId> owners;
        for (auto const& t : tempFiles) owners.push_back(metaOwner(t));
        std::sort(owners.begin(), owners.end());
        bool const ownershipOk = owners.size() == 2 && owners[0] == idA && owners[1] == idB;
        check(ownershipOk, "S6 each temp file is owned by its own task (meta taskId)");

        // 唯一化得到的临时名主干应已回写并经 onFileNameChanged 上报，与磁盘 .edm 一致
        bool const tempSyncOk = [&] {
            for (auto const& t : tempFiles) {
                auto base = t.filename().string();
                base.resize(base.size() - 4); // 去 .edm
                if (observer.fileNameOf(metaOwner(t)) != base) return false;
            }
            return true;
        }();
        check(tempSyncOk, "S6 unique temp base synced to task fileName");

        check(static_cast<bool>(dispatcher.resumeTask(idA)), "S6 resume A");
        check(static_cast<bool>(dispatcher.resumeTask(idB)), "S6 resume B");
        bool const finished = waitTerminal(dispatcher, idA, 180) && waitTerminal(dispatcher, idB, 180);
        check(finished, "S6 both tasks finished");

        // 完成后：无临时文件残留；重名按 name(1).ext 序号落地，原始名保持干净
        std::vector<std::string> finals;
        for (auto const& entry : fs::directory_iterator(".tmp/engine/s6/content")) {
            finals.push_back(entry.path().filename().string());
        }
        std::sort(finals.begin(), finals.end());
        bool const noTemp = std::none_of(finals.begin(), finals.end(), [](std::string const& n) {
            return n.ends_with(".edm") || n.ends_with(".meta");
        });
        check(noTemp, "S6 no .edm/.meta leftover after finish");
        bool const finalOk = finals.size() == 2 && finals[0] == "e2e-test(1).bin" && finals[1] == "e2e-test.bin";
        for (auto const& f : finals) std::printf("      final: %s\n", f.c_str());
        check(finalOk, "S6 same-name finals use Windows counter naming (name(1).ext)");

        bool const md5Ok = verifyFile(".tmp/engine/s6/content/e2e-test.bin", md5)
                        && verifyFile(".tmp/engine/s6/content/e2e-test(1).bin", md5);
        check(md5Ok, "S6 both files MD5 match ground truth");

        // 完成重命名后的最终名同样应回写并上报，记录名集合与落盘名集合一致
        std::vector<std::string> synced = {observer.fileNameOf(idA), observer.fileNameOf(idB)};
        std::sort(synced.begin(), synced.end());
        bool const finalSyncOk = synced == finals;
        for (auto const& s : synced) std::printf("      synced name: %s\n", s.c_str());
        check(finalSyncOk, "S6 final names synced to task fileName");
        if (!(pausedA && pausedB && namingOk && ownershipOk && tempSyncOk && finished && noTemp && finalOk && md5Ok
              && finalSyncOk))
            ++failures;
    }

    std::printf("\n===== engine test summary =====\n");
    std::printf("failures: %d\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
