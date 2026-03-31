cmake_minimum_required(VERSION 3.16)

project(EasyDownloadManager VERSION 0.1 LANGUAGES CXX)

# --- 基础设置 ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# --- 查找依赖 ---
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(magic_enum CONFIG REQUIRED)
find_package(SQLiteCpp REQUIRED)
find_package(CURL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(expected-lite CONFIG REQUIRED)

# --- 源文件列表 ---
set(PROJECT_SOURCES
        src/components/ThreadProgressBar.cc
        src/components/ThreadProgressBar.h
        src/EdmConfig.cc
        src/EdmConfig.h
        src/database/DownloadDatabase.cc
        src/database/DownloadDatabase.h
        src/downloader/DownloadTask.cc
        src/downloader/DownloadTask.h
        src/downloader/DownloadWorker.cc
        src/downloader/DownloadWorker.h
        src/downloader/TaskConfigure.h
        src/downloader/TaskConfigure.cc
        src/downloader/MetaInfoFetcher.cc
        src/downloader/MetaInfoFetcher.h
        src/event/EventBus.h
        src/event/SignalHandler.cc
        src/event/SignalHandler.h
        src/Global.h
        src/model/TaskModel.h
        src/resources/Resources.qrc
        src/ui/main/MainWindow.cc
        src/ui/main/MainWindow.h
        src/ui/main/MainWindow.ui
        src/ui/new_task/NewTaskDialog.h
        src/ui/new_task/NewTaskDialog.cc
        src/ui/new_task/NewTaskDialog.ui
        src/ui/settings/SettingsDialog.h
        src/ui/settings/SettingsDialog.cc
        src/ui/settings/SettingsDialog.ui
        src/ui/task_completed/TaskCompletedDialog.h
        src/ui/task_completed/TaskCompletedDialog.cc
        src/ui/task_completed/TaskCompletedDialog.ui
        src/ui/task_downloading/TaskDownloadingDialog.h
        src/ui/task_downloading/TaskDownloadingDialog.cc
        src/ui/task_downloading/TaskDownloadingDialog.ui
        src/ui/task_information/TaskInformationDialog.h
        src/ui/task_information/TaskInformationDialog.cc
        src/ui/task_information/TaskInformationDialog.ui
        src/utils/EnumUtils.h
        src/utils/IconUtils.h
        src/utils/StringUtils.h
        src/utils/Utils.h
        src/Dispatcher.cc
        src/Dispatcher.h
        src/EdmApplication.cc
        src/EdmApplication.h
        src/main.cpp
        src/event/Events.h
        src/expected.h
        src/downloader/CurlEx.h
        src/downloader/CurlEx.cc
        src/downloader/FetchedMetaInfo.h
)

# --- 创建目标 ---
if (QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(EasyDownloadManager MANUAL_FINALIZATION ${PROJECT_SOURCES})
else()
    add_executable(EasyDownloadManager ${PROJECT_SOURCES})
endif()

# --- 包含目录 ---
target_include_directories(EasyDownloadManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- 编译定义 ---
target_compile_definitions(EasyDownloadManager PRIVATE
        NOMINMAX
        UNICODE
        _UNICODE
        CURL_STATICLIB
        $<$<CONFIG:Debug>:EDM_DEBUG>
)

# --- 编译选项 ---
if (MSVC)
    target_compile_options(EasyDownloadManager PRIVATE /utf-8 /W4 /sdl)
    # 自动切换 MD/MDd
    set_property(TARGET EasyDownloadManager PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(EasyDownloadManager PRIVATE -Wall -Wextra)
endif()

# --- 链接库 ---
target_link_libraries(EasyDownloadManager PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        magic_enum::magic_enum
        SQLiteCpp
        CURL::libcurl
        fmt::fmt
)

# --- 平台特定设置 ---
set_target_properties(EasyDownloadManager PROPERTIES
        WIN32_EXECUTABLE FALSE # 设为 TRUE 则隐藏控制台，设为 FALSE 方便看 qDebug
        MACOSX_BUNDLE TRUE
)

# --- Qt 最终化 ---
if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(EasyDownloadManager)
endif()