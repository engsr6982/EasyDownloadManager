add_executable(edm-engine-test engine_test.cc)

set_target_properties(edm-engine-test PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_link_libraries(edm-engine-test PRIVATE src-downloader)

if (MSVC)
    target_compile_options(edm-engine-test PRIVATE /utf-8 /W4 /sdl)
    set_property(TARGET edm-engine-test PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(edm-engine-test PRIVATE -Wall -Wextra)
endif()
