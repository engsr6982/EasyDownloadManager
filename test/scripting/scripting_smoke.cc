/**
 * 脚本扩展系统端到端冒烟测试（无 GUI）。
 *
 * 在 QCoreApplication 事件循环上以 Qt 适配服务启动 AppService + ScriptingBootstrap
 * 全链路，指向 test/server 的测试文件服务器创建下载任务，验证：
 *   1. 脚本发现/manifest 校验/独立引擎加载（含 onLoad 抛错脚本的错误隔离）；
 *   2. beforeCreateTask / beforeFetchMeta / afterFetchMeta 可变钩子（含异步 Promise 改写）；
 *   3. taskStarted / taskProgress / taskCompleted 通知钩子；
 *   4. 单脚本异常不影响其他脚本与下载。
 *
 * 环境：
 *   EDM_SCRIPT_DIR    脚本目录
 *   EDM_TEST_URL      测试文件 URL（默认 http://127.0.0.1:3187/file）
 *   EDM_TEST_DIR      下载保存目录（默认 .tmp/e2e-download）
 *
 * 输出：各脚本虚拟控制台内容 + 结果汇总，退出码 0 表示全部通过。
 */

#include "AppService.h"
#include "EdmConfig.h"
#include "database/TaskModel.h"
#include "scripting/api/ScriptConsole.h"
#include "scripting/engine/ScriptingBootstrap.h"
#include "scripting/qt/ScriptQtHost.h"

#include <QCoreApplication>
#include <QTimer>

#include <cstdio>
#include <filesystem>
#include <memory>

#ifdef _WIN32
#include <crtdbg.h>
#endif

using edm::AppService;
using edm::script::DiscoveredScript;
using edm::script::NotifyLevel;
using edm::script::ScriptConsole;
using edm::script::ScriptHostServices;
using edm::script::ScriptingBootstrap;

namespace {

void dumpConsole(ScriptingBootstrap& bootstrap, std::string const& name) {
    auto* console = bootstrap.console(name);
    std::printf("\n===== [%s] console =====\n", name.c_str());
    if (!console) {
        std::printf("(no console)\n");
        return;
    }
    for (auto const& entry : console->entries()) {
        std::printf("%s [%d] %s\n", entry.time.c_str(), entry.level, entry.message.c_str());
    }
}

int report(ScriptingBootstrap& bootstrap, bool downloadOk) {
    auto scripts = bootstrap.discoverScripts();
    bool loggerOk{false};
    bool rewriterOk{false};
    bool asyncOk{false};
    bool errorIsolated{true};
    int  errorCount{0};

    auto hasMessage = [&](std::string const& name, std::string const& needle) {
        auto* console = bootstrap.console(name);
        if (!console) return false;
        for (auto const& entry : console->entries()) {
            if (entry.message.find(needle) != std::string::npos) return true;
        }
        return false;
    };

    for (auto const& script : scripts) {
        auto const& name = script.manifest.name;
        dumpConsole(bootstrap, name);
        if (script.error) ++errorCount;
        if (name == "download-logger" && !script.error) {
            if (hasMessage(name, "开始下载") && hasMessage(name, "下载完成")) loggerOk = true;
        }
        if (name == "url-rewriter" && !script.error) {
            if (hasMessage(name, "afterFetchMeta")) rewriterOk = true;
        }
        if (name == "async-rewriter" && !script.error) {
            if (hasMessage(name, "async rewrite ok")) asyncOk = true;
        }
        if (name == "download-logger" && script.error) {
            errorIsolated = false; // 正常脚本被坏脚本波及
        }
    }

    std::printf("\n===== summary =====\n");
    std::printf("download finished : %s\n", downloadOk ? "PASS" : "FAIL");
    std::printf("notify hooks      : %s\n", loggerOk ? "PASS" : "FAIL");
    std::printf("afterFetchMeta    : %s\n", rewriterOk ? "PASS" : "FAIL");
    std::printf("async rewrite     : %s\n", asyncOk ? "PASS" : "FAIL");
    std::printf("error isolation   : %s (error scripts: %d)\n", errorIsolated ? "PASS" : "FAIL", errorCount);

    return (downloadOk && loggerOk && rewriterOk && asyncOk && errorIsolated && errorCount == 1) ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    // 自动化环境：CRT assert 输出到 stderr（MSVC 默认弹模态对话框会挂住无人值守运行）
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    QCoreApplication app(argc, argv);

    auto const testUrl = qEnvironmentVariable("EDM_TEST_URL", QStringLiteral("http://127.0.0.1:3187/file"));
    auto const saveDir = qEnvironmentVariable("EDM_TEST_DIR", QStringLiteral(".tmp/e2e-download"));

    std::filesystem::create_directories(saveDir.toStdString());

    AppService         service;
    ScriptingBootstrap bootstrap;
    service.setScripting(&bootstrap);

    // 宿主服务：Qt 定时器/事件泵 + 测试打印
    edm::script::qt::QtHostServices services{&app};
    services.scriptsDirValue    = qEnvironmentVariable("EDM_SCRIPT_DIR").toStdString();
    services.appDataDirValue    = QStringLiteral(".tmp/e2e-appdata").toStdString();
    services.hookTimeoutMsValue = 10000;
    services.userAgentValue     = "edm-scripting-smoke/1.0";
    services.notifyHandler = [](NotifyLevel level, std::string const& title, std::string const& message) {
        std::printf("[notify][level=%d] %s: %s\n", static_cast<int>(level), title.c_str(), message.c_str());
    };
    services.warningHandler = [](std::string const& message) { std::fprintf(stderr, "%s\n", message.c_str()); };
    services.disabledScriptsHandler = [] { return std::vector<std::string>{}; };
    services.setDisabledScriptsHandler = [](std::vector<std::string> const&) {};
    services.taskGetHandler = [&service](int id) { return service.taskSnapshot(id); };
    services.taskGetAllHandler = [&service] { return service.taskSnapshots(); };
    services.taskControlHandler = [&service](int id, edm::script::TaskAction action) {
        return service.taskAction(id, action);
    };

    bootstrap.initialize(&services);

    auto downloadOk = std::make_shared<bool>(false);
    auto finished   = std::make_shared<bool>(false);

    // 任务终态：Finished 才算通过
    QObject::connect(
        &service,
        &AppService::taskFinished,
        &app,
        [downloadOk, finished, &app](int, int state, QString const&, QString const&) {
            if (*finished) return;
            *finished   = true;
            *downloadOk = (static_cast<edm::TaskState>(state) == edm::TaskState::Finished);
            QTimer::singleShot(300, &app, [&app] { app.quit(); }); // 留出最后一批通知
        }
    );

    // 兜底超时：120s
    QTimer::singleShot(120000, &app, [&app, finished, downloadOk] {
        if (!*finished) {
            std::fprintf(stderr, "TIMEOUT: task did not finish in 120s\n");
            *downloadOk = false;
            *finished   = true;
        }
        app.quit();
    });

    auto created = service.createTask(testUrl, saveDir, false);
    if (!created) {
        std::fprintf(stderr, "createTask failed: %s\n", created.error().message().c_str());
        return 1;
    }
    std::printf("task created: %d url=%s\n", (*created)->id, testUrl.toStdString().c_str());
    std::fflush(stdout);

    app.exec();

    return report(bootstrap, *downloadOk);
}
