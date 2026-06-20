cmake_minimum_required(VERSION 3.16)

project(libdownloader VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(expected-lite CONFIG REQUIRED)

set(LIBDOWNLOADER_SOURCES
        src/Dispatcher.cc
        src/Dispatcher.h
        src/Global.h
        src/dto/TaskContext.h
        src/expected.h
        src/model/TaskModel.h
        src/utils/TaskLogger.h
        src/utils/TaskLogger.cc
        src/downloader/DownloadTask.cc
        src/downloader/DownloadTask.h
        src/downloader/DownloadWorker.cc
        src/downloader/DownloadWorker.h
        src/downloader/DownloadState.h
        src/downloader/CurlEx.h
        src/downloader/CurlEx.cc
        src/downloader/FetchedMetaInfo.h
        src/downloader/FileVerifier.cc
        src/downloader/FileVerifier.h
        src/downloader/TaskConfigure.h
        src/downloader/TaskConfigure.cc
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

target_include_directories(libdownloader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
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
        nonstd::expected-lite
)

if (WIN32)
    target_link_libraries(libdownloader PUBLIC bcrypt)
endif()

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
