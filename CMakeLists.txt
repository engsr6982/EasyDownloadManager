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
        src/windows/main/MainWindow.cc
        src/windows/main/MainWindow.h
        src/windows/main/MainWindow.ui
        src/EasyDownloadManager.cc
        src/EasyDownloadManager.h
)

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

target_include_directories(EasyDownloadManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(EasyDownloadManager PRIVATE
        $<$<CONFIG:Debug>:EDM_DEBUG>
)

target_link_libraries(EasyDownloadManager PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

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
