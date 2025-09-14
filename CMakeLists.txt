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
)

# ---------------------------------------------------------
# XMAKE packages base paths (change these only once)
# ---------------------------------------------------------
set(XMAKE_MAGIC_ENUM_DIR "C:/Users/X/AppData/Local/.xmake/packages/m/magic_enum/v0.9.7/03e2990032db4ea08102d17ff8a57da7")
set(XMAKE_LIBCURL_DIR    "C:/Users/X/AppData/Local/.xmake/packages/l/libcurl/8.11.0/76d551ce47c74312a891bcb6ab3875e4")
set(XMAKE_ZLIB_DIR       "C:/Users/X/AppData/Local/.xmake/packages/z/zlib/v1.3.1/95786712bac941e795afa187a68143a4")
set(XMAKE_OPENSSL3_DIR   "C:/Users/X/AppData/Local/.xmake/packages/o/openssl3/3.5.1/e2851f95acb4466da9a58b4aabd95f79")



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
        ${XMAKE_LIBCURL_DIR}/include
        ${XMAKE_ZLIB_DIR}/include
        ${XMAKE_OPENSSL3_DIR}/include
)

target_compile_definitions(EasyDownloadManager PRIVATE
        $<$<CONFIG:Debug>:EDM_DEBUG>
)

target_link_libraries(EasyDownloadManager PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_directories(EasyDownloadManager PRIVATE
        ${XMAKE_LIBCURL_DIR}/lib
        ${XMAKE_ZLIB_DIR}/lib
        ${XMAKE_OPENSSL3_DIR}/lib
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

# Dummy target (avoid building with CMake accidentally)
add_custom_target(dummy ALL
        COMMAND ${CMAKE_COMMAND} -E echo ">>> Use xmake to build, CMake is for CLion IDE only")