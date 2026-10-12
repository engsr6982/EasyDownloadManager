add_executable(src-downloader-cli cli_main.cc)

set_target_properties(src-downloader-cli PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_link_libraries(src-downloader-cli PRIVATE src-downloader)

if (MSVC)
    target_compile_options(src-downloader-cli PRIVATE /utf-8 /W4 /sdl)
    set_property(TARGET src-downloader-cli PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(src-downloader-cli PRIVATE -Wall -Wextra)
endif()