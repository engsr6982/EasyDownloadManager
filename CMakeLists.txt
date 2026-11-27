cmake_minimum_required(VERSION 3.16)

# vcpkg 缓存收敛进项目（须在 project() 之前设置，toolchain 在 configure 期消费）：
# 二进制缓存与源码/工具下载缓存不出仓库，不写用户全局目录
set(EDM_VCPKG_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/.vcpkg-cache")
file(MAKE_DIRECTORY "${EDM_VCPKG_CACHE}/binary")
file(MAKE_DIRECTORY "${EDM_VCPKG_CACHE}/downloads")
set(ENV{VCPKG_DEFAULT_BINARY_CACHE} "${EDM_VCPKG_CACHE}/binary")
set(ENV{VCPKG_DOWNLOADS} "${EDM_VCPKG_CACHE}/downloads")

project(EasyDownloadManager VERSION 0.1 LANGUAGES CXX)

# --- 基础设置 ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- 查找全局依赖 ---
find_package(Qt6 REQUIRED COMPONENTS Widgets Quick QuickControls2 QuickLayouts QuickDialogs2 Network WebSockets)
find_package(magic_enum CONFIG REQUIRED)
find_package(SQLiteCpp REQUIRED)
find_package(CURL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED)

# --- 脚本扩展系统依赖：quickjs-ng + jspp（jspp 不自带引擎，宿主负责拉取并链接） ---
include(FetchContent)
# jspp/quickjs 均跟踪 main 分支：首次配置时从 GitHub 拉取，此后使用本地副本构建
# （避免每次构建都 git fetch 远端；需要更新时删除 _deps 目录重新配置即可）
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(
        quickjs
        GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
        GIT_TAG        v0.16.2
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(quickjs)

set(JSPP_BACKEND quickjs CACHE STRING "" FORCE)
FetchContent_Declare(
        jspp
        GIT_REPOSITORY https://github.com/engsr6982/jspp.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(jspp)
target_link_libraries(jspp PUBLIC qjs) # jspp 约定：引擎库由宿主 PUBLIC 链接（头文件暴露 quickjs.h）

# --- 引入子目录 ---
add_subdirectory(src-common)
add_subdirectory(src-downloader)
add_subdirectory(test/cli)
add_subdirectory(test/scripting)
add_subdirectory(test/engine)

# --- 主应用源文件列表 ---
set(PROJECT_SOURCES
        src/EdmConfig.cc
        src/EdmConfig.h
        src/AppService.cc
        src/AppService.h
        src/database/DownloadDatabase.cc
        src/database/DownloadDatabase.h
        src/qml/QmlTaskListModel.cc
        src/qml/QmlTaskListModel.h
        src/qml/QmlTaskDetail.cc
        src/qml/QmlTaskDetail.h
        src/qml/QmlSettings.cc
        src/qml/QmlSettings.h
        src/qml/QmlController.cc
        src/qml/QmlController.h
        src/qml/CommandRegistry.cc
        src/qml/CommandRegistry.h
        src/qml/QmlScriptManager.cc
        src/qml/QmlScriptManager.h
        src/qml/ScriptLogModel.cc
        src/qml/ScriptLogModel.h
        src/qml/QmlWsServer.cc
        src/qml/QmlWsServer.h
        src/ws/WsPacket.h
        src/ws/WsPackets.h
        src/ws/WsServer.cc
        src/ws/WsServer.h
        src/ws/WsBridge.cc
        src/ws/WsBridge.h
        src/scripting/api/ScriptConsole.cc
        src/scripting/api/ScriptConsole.h
        src/scripting/api/ScriptEvents.h
        src/scripting/api/ScriptFs.cc
        src/scripting/api/ScriptFs.h
        src/scripting/api/ScriptHostServices.h
        src/scripting/api/ScriptHttp.cc
        src/scripting/api/ScriptHttp.h
        src/scripting/api/ScriptManifest.cc
        src/scripting/api/ScriptManifest.h
        src/scripting/engine/ScriptEngineState.cc
        src/scripting/engine/ScriptEngineState.h
        src/scripting/engine/ScriptEventBus.cc
        src/scripting/engine/ScriptEventBus.h
        src/scripting/engine/ScriptingBootstrap.cc
        src/scripting/engine/ScriptingBootstrap.h
        src/scripting/qt/ScriptQtHost.cc
        src/scripting/qt/ScriptQtHost.h
        src/scripting/binding/ScriptBindings.cc
        src/scripting/binding/ScriptBindings.h
        src/utils/EnumUtils.h
        src/EdmApplication.cc
        src/EdmApplication.h
        src/main.cpp
        Assets.qrc
        src/FramelessShadowFilter.cc
        src/FramelessShadowFilter.h
)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets.rc")
    list(APPEND PROJECT_SOURCES Assets.rc)
endif()

# --- 创建主应用目标 ---
qt_add_executable(EasyDownloadManager MANUAL_FINALIZATION ${PROJECT_SOURCES})

# --- 显式设置 QTP0004 策略为 OLD，忽略子目录缺少 qmldir 的警告 ---
qt_policy(SET QTP0004 NEW)

# --- QML 模块（在根目录定义，完美对接 src-ui 相对路径） ---
qt_add_qml_module(EasyDownloadManager
        URI "edm.qml.ui"
        VERSION 1.0
        RESOURCE_PREFIX "/qt/qml"

        QML_FILES
        src-ui/Src/App.qml
        src-ui/Src/Constants.qml
        src-ui/Src/views/TaskListView.qml
        src-ui/Src/views/SettingsView.qml
        src-ui/Src/views/ExtensionView.qml
        src-ui/Src/components/NavButton.qml
        src-ui/Src/components/ScrollToTopButton.qml
        src-ui/Src/components/TaskCard.qml
        src-ui/Src/components/ExtensionCard.qml
        src-ui/Src/dialog/NewTaskDialog.qml
        src-ui/Src/dialog/DeleteTaskDialog.qml
        src-ui/Src/dialog/ClearTasksDialog.qml
        src-ui/Src/dialog/UnInstallExtensionDialog.qml
        src-ui/Src/dialog/ConsoleWindow.qml
        src-ui/Src/dialog/CommandPalette.qml
        src-ui/Src/drawer/TaskDetailDrawer.qml
        src-ui/Src/fluent/FluButton.qml
        src-ui/Src/fluent/FluCheckBox.qml
        src-ui/Src/fluent/FluComboBox.qml
        src-ui/Src/fluent/FluIconButton.qml
        src-ui/Src/fluent/FluIndicator.qml
        src-ui/Src/fluent/FluMenu.qml
        src-ui/Src/fluent/FluMenuItem.qml
        src-ui/Src/fluent/FluMenuSeparator.qml
        src-ui/Src/fluent/FluPopup.qml
        src-ui/Src/fluent/FluProgressBar.qml
        src-ui/Src/fluent/FluSettingExpander.qml
        src-ui/Src/fluent/FluSettingGroup.qml
        src-ui/Src/fluent/FluSettingRow.qml
        src-ui/Src/fluent/FluSwitch.qml
        src-ui/Src/fluent/FluText.qml
        src-ui/Src/fluent/FluTextArea.qml
        src-ui/Src/fluent/FluTextField.qml

        RESOURCES
        src-ui/Src/qmldir
        src-ui/Src/fonts/FluentSystemIcons-Regular.ttf
        src-ui/Src/fonts/FluentSystemIcons-Regular.json
        src-ui/Src/fonts/SourceHanSansCN-Regular.otf
)

# --- 包含目录 ---
target_include_directories(EasyDownloadManager PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/qml
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scripting
)

# --- 编译定义 ---
target_compile_definitions(EasyDownloadManager PRIVATE
        UNICODE
        _UNICODE
        $<$<CONFIG:Debug>:EDM_DEBUG>
)

# --- 编译选项 ---
if (MSVC)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(EasyDownloadManager PRIVATE /utf-8 /W4 /sdl -Wno-unused-command-line-argument)
    else()
        target_compile_options(EasyDownloadManager PRIVATE /utf-8 /W4 /sdl)
    endif()
    set_property(TARGET EasyDownloadManager PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(EasyDownloadManager PRIVATE -Wall -Wextra)
endif()

# --- 链接库 ---
target_link_libraries(EasyDownloadManager PRIVATE
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
        Qt6::WebSockets
        src-downloader
        edm-common
        magic_enum::magic_enum
        SQLiteCpp
        jspp
)

if (MSVC)
    target_link_libraries(EasyDownloadManager PRIVATE
            dwmapi
    )
endif()

# --- 平台特定设置 ---
set_target_properties(EasyDownloadManager PROPERTIES
        WIN32_EXECUTABLE FALSE
        MACOSX_BUNDLE TRUE
)

qt_finalize_executable(EasyDownloadManager)