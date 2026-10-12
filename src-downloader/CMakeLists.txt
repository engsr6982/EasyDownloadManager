set(DOWNLOADER_SOURCES
        Dispatcher.cc
        Dispatcher.h
        ITaskObserver.h
        Global.h
        expected.h
        utils/TaskLogger.h
        utils/TaskLogger.cc
        downloader/DownloadTypes.h
        downloader/DownloadTask.cc
        downloader/DownloadTask.h
        downloader/DownloadWorker.cc
        downloader/DownloadWorker.h
        downloader/CurlEx.h
        downloader/CurlEx.cc
        downloader/FileVerifier.cc
        downloader/FileVerifier.h
        downloader/MetaInfoFetcher.cc
        downloader/MetaInfoFetcher.h
        schema/ChunkMetaFile.h
)

add_library(src-downloader STATIC ${DOWNLOADER_SOURCES})
add_library(src-downloader::src-downloader ALIAS src-downloader)

set_target_properties(src-downloader PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(src-downloader PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(src-downloader PUBLIC
        NOMINMAX
        CURL_STATICLIB
        SPDLOG_FMT_EXTERNAL
)

if (MSVC)
    target_compile_options(src-downloader PRIVATE /utf-8 /W4 /sdl)
    set_property(TARGET src-downloader PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(src-downloader PRIVATE -Wall -Wextra)
endif()

target_link_libraries(src-downloader PUBLIC
        CURL::libcurl
        fmt::fmt
        spdlog::spdlog
        glaze::glaze
)

if (WIN32)
    target_link_libraries(src-downloader PUBLIC bcrypt)
endif()