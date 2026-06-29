cmake_minimum_required(VERSION 3.16)

project(libdownloader VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(CURL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED)

set(LIBDOWNLOADER_SOURCES
        src/Dispatcher.cc
        src/Dispatcher.h
        src/Global.h
        src/expected.h
        src/utils/TaskLogger.h
        src/utils/TaskLogger.cc
        src/downloader/DownloadTypes.h
        src/downloader/DownloadTask.cc
        src/downloader/DownloadTask.h
        src/downloader/DownloadWorker.cc
        src/downloader/DownloadWorker.h
        src/downloader/CurlEx.h
        src/downloader/CurlEx.cc
        src/downloader/FileVerifier.cc
        src/downloader/FileVerifier.h
        src/downloader/MetaInfoFetcher.cc
        src/downloader/MetaInfoFetcher.h
)

add_library(libdownloader STATIC ${LIBDOWNLOADER_SOURCES})
add_library(libdownloader::libdownloader ALIAS libdownloader)

set_target_properties(libdownloader PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(libdownloader PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(libdownloader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(libdownloader PUBLIC
        NOMINMAX
        CURL_STATICLIB
        SPDLOG_FMT_EXTERNAL
)

if (MSVC)
    target_compile_options(libdownloader PRIVATE /utf-8 /W4 /sdl)
    set_property(TARGET libdownloader PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(libdownloader PRIVATE -Wall -Wextra)
endif()

target_link_libraries(libdownloader PUBLIC
        CURL::libcurl
        fmt::fmt
        spdlog::spdlog
        glaze::glaze
)

if (WIN32)
    target_link_libraries(libdownloader PUBLIC bcrypt)
endif()

# 1. 安装生成的静态库（.lib）并关联到一个导出集
install(TARGETS libdownloader
    EXPORT libdownloader-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 2. 安装整个 src 目录下的头文件，并完美保留其子目录结构（如 utils/, downloader/）
install(DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h"
)

# 3. 导出 CMake 配置文件，使得主项目可以通过 find_package(libdownloader CONFIG REQUIRED) 完美链接
install(EXPORT libdownloader-targets
    FILE libdownloaderTargets.cmake
    NAMESPACE libdownloader::
    DESTINATION share/libdownloader
)

set(LIBDOWNLOADER_IS_TOP_LEVEL OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(LIBDOWNLOADER_IS_TOP_LEVEL ON)
endif()
option(LIBDOWNLOADER_BUILD_TESTS "Build libdownloader CLI test targets" ${LIBDOWNLOADER_IS_TOP_LEVEL})

if (LIBDOWNLOADER_BUILD_TESTS)
    add_executable(libdownloader_cli EXCLUDE_FROM_ALL test/cli/cli_main.cc)
    target_link_libraries(libdownloader_cli PRIVATE libdownloader)
    if (MSVC)
        target_compile_options(libdownloader_cli PRIVATE /utf-8 /W4 /sdl)
        set_property(TARGET libdownloader_cli PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        target_compile_options(libdownloader_cli PRIVATE -Wall -Wextra)
    endif()
endif()
