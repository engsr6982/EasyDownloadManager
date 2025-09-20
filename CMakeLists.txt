# Use xmake to build, CMake is for CLion IDE only

cmake_minimum_required(VERSION 3.16)

project(EasyDownloadManager VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        src/main.cpp
        src/resources/Icon.rc
        src/resources/Resources.qrc
        src/utils/IconUtils.h
        src/ui/main/MainWindow.cc
        src/ui/main/MainWindow.h
        src/ui/main/MainWindow.ui
        src/EasyDownloadManager.cc
        src/EasyDownloadManager.h
        src/ui/new_task/NewTaskDialog.h
        src/ui/new_task/NewTaskDialog.cc
        src/ui/new_task/NewTaskDialog.ui
        src/Dispatcher.cc
        src/Dispatcher.h
        src/event/EventBus.h
        src/event/SignalHandler.cc
        src/event/SignalHandler.h
        src/ui/settings/SettingsDialog.h
        src/ui/settings/SettingsDialog.cc
        src/ui/settings/SettingsDialog.ui
        src/config/EdmGlobalConfig.cc
        src/config/EdmGlobalConfig.h
        src/utils/StringUtils.h
        src/utils/EnumUtils.h
        src/downloader/MultiThreadDownloader.cc
        src/downloader/MultiThreadDownloader.h
        src/downloader/DownloadWorker.cc
        src/downloader/DownloadWorker.h
        src/downloader/DownloadTask.cc
        src/downloader/DownloadTask.h
        src/downloader/TaskMetaInfoFetcher.cc
        src/downloader/TaskMetaInfoFetcher.h
        src/downloader/TaskConfigure.h
        src/downloader/TaskConfigure.cc
        src/utils/ProxyUtils.h
        src/database/DownloadDatabase.cc
        src/database/DownloadDatabase.h
        src/model/TaskModel.h
        src/model/GlobalDefaults.h
        src/model/Definition.h
        src/ui/task_completed/TaskCompletedDialog.h
        src/ui/task_completed/TaskCompletedDialog.cc
        src/ui/task_completed/TaskCompletedDialog.ui
        src/ui/task_downloading/TaskDownloadingDialog.h
        src/ui/task_downloading/TaskDownloadingDialog.cc
        src/ui/task_downloading/TaskDownloadingDialog.ui
        src/ui/task_information/TaskInformationDialog.h
        src/ui/task_information/TaskInformationDialog.cc
        src/ui/task_information/TaskInformationDialog.ui
        src/components/ThreadProgressBar.cc
        src/components/ThreadProgressBar.h
)

# ---------------------------------------------------------
# XMAKE packages base paths (change these only once)
# ---------------------------------------------------------
set(XMAKE_MAGIC_ENUM_DIR "C:/Users/X/AppData/Local/.xmake/packages/m/magic_enum/v0.9.7/03e2990032db4ea08102d17ff8a57da7")

list(APPEND CMAKE_PREFIX_PATH
        "C:/Users/X/AppData/Local/.xmake/packages/l/libcurl/8.11.0/0a2d37d3141d4b51ba365acba6fe9f14"
        "C:/Users/X/AppData/Local/.xmake/packages/z/zlib/v1.3.1/553aa759ee6a4f56b40e79352ddd3cd1"
        "C:/Users/X/AppData/Local/.xmake/packages/s/sqlitecpp/3.3.3/b091148d1dc14450be8cfe4390bc4640"
)
## Release
#list(APPEND CMAKE_PREFIX_PATH
#        "C:/Users/X/AppData/Local/.xmake/packages/l/libcurl/8.11.0/9cbd24f6741f439684e839060e1c6d11"
#        "C:/Users/X/AppData/Local/.xmake/packages/z/zlib/v1.3.1/95786712bac941e795afa187a68143a4"
#        "C:/Users/X/AppData/Local/.xmake/packages/s/sqlitecpp/3.3.3/33c7042408f04a3ab8ca7543f3216206"
#)

find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SQLiteCpp REQUIRED)

if (WIN32 AND MSVC)
    message(STATUS "Using Schannel on Windows (MSVC)")
else ()
    message(STATUS "Using OpenSSL3")
    list(APPEND CMAKE_PREFIX_PATH "C:/Users/X/AppData/Local/.xmake/packages/o/openssl3/3.5.1/e2851f95acb4466da9a58b4aabd95f79")
    find_package(OpenSSL REQUIRED)
endif ()


if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(EasyDownloadManager
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
    )
    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET EasyDownloadManager APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else ()
    if (ANDROID)
        add_library(EasyDownloadManager SHARED
                ${PROJECT_SOURCES}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else ()
        add_executable(EasyDownloadManager
                ${PROJECT_SOURCES}
        )
    endif ()
endif ()

target_include_directories(EasyDownloadManager PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${XMAKE_MAGIC_ENUM_DIR}/include
)

target_compile_definitions(EasyDownloadManager PRIVATE $<$<CONFIG:Debug>:EDM_DEBUG>)
target_compile_definitions(EasyDownloadManager PRIVATE CURL_STATICLIB)

if (MSVC)
    # MSVC Runtime (/MD /MDd)
    set_property(TARGET EasyDownloadManager PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # 编译选项
    target_compile_options(EasyDownloadManager PRIVATE
            /utf-8
            /W4
            /sdl
    )
    # 宏定义
    target_compile_definitions(EasyDownloadManager PRIVATE
            NOMINMAX
            UNICODE
    )
endif ()

target_link_libraries(EasyDownloadManager PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        CURL::libcurl
        SQLiteCpp
)


# ---------------------------------------------------------
# Bundle / install settings (mostly Qt boilerplate)
# ---------------------------------------------------------
# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if (${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.EasyDownloadManager)
endif ()
set_target_properties(EasyDownloadManager PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE FALSE # fix qDebug() not working on Windows
)

include(GNUInstallDirs)
install(TARGETS EasyDownloadManager
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(EasyDownloadManager)
endif ()
