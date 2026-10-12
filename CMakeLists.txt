cmake_minimum_required(VERSION 3.16)
project(EasyDownloadManager VERSION 0.1 LANGUAGES CXX)

# --- 基础设置 ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- 查找全局依赖 ---
find_package(Qt6 REQUIRED COMPONENTS Widgets Quick QuickControls2 QuickLayouts QuickDialogs2)
find_package(magic_enum CONFIG REQUIRED)
find_package(SQLiteCpp REQUIRED)
find_package(CURL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED)

# --- 引入下载引擎子目录 ---
add_subdirectory(src-downloader)
add_subdirectory(test/cli)

# --- 主应用源文件列表 ---
set(PROJECT_SOURCES
        src/EdmConfig.cc
        src/EdmConfig.h
        src/database/DownloadDatabase.cc
        src/database/DownloadDatabase.h
        src/event/EventBus.h
        src/event/SignalHandler.cc
        src/event/SignalHandler.h
        src/qml/QmlTaskListModel.cc
        src/qml/QmlTaskListModel.h
        src/qml/QmlTaskDetail.cc
        src/qml/QmlTaskDetail.h
        src/qml/QmlSettings.cc
        src/qml/QmlSettings.h
        src/qml/QmlController.cc
        src/qml/QmlController.h
        src/utils/EnumUtils.h
        src/utils/Utils.h
        src/EdmApplication.cc
        src/EdmApplication.h
        src/main.cpp
        src/utils/TimeUtils.h
        Assets.qrc
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
        src-ui/Src/components/TaskCard.qml
        src-ui/Src/dialog/NewTaskDialog.qml
        src-ui/Src/dialog/DeleteTaskDialog.qml
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
        src-downloader
        magic_enum::magic_enum
        SQLiteCpp
)

# --- 平台特定设置 ---
set_target_properties(EasyDownloadManager PROPERTIES
        WIN32_EXECUTABLE FALSE
        MACOSX_BUNDLE TRUE
)

qt_finalize_executable(EasyDownloadManager)