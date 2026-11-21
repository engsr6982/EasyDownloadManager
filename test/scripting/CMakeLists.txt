# 脚本扩展系统端到端冒烟测试（无 GUI）
# 复用 app 层源码（AppService / database / scripting），复用 test/server 的测试文件服务器。
set(SCRIPTING_TEST_SOURCES
        scripting_smoke.cc
        ${CMAKE_SOURCE_DIR}/src/AppService.cc
        ${CMAKE_SOURCE_DIR}/src/AppService.h
        ${CMAKE_SOURCE_DIR}/src/EdmConfig.cc
        ${CMAKE_SOURCE_DIR}/src/EdmConfig.h
        ${CMAKE_SOURCE_DIR}/src/database/DownloadDatabase.cc
        ${CMAKE_SOURCE_DIR}/src/database/DownloadDatabase.h
        ${CMAKE_SOURCE_DIR}/src/database/TaskModel.h
        ${CMAKE_SOURCE_DIR}/src/database/TaskModelConversion.h
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptConsole.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptConsole.h
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptEvents.h
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptFs.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptFs.h
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptHttp.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptHttp.h
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptManifest.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/api/ScriptManifest.h
        ${CMAKE_SOURCE_DIR}/src/scripting/engine/ScriptEngineState.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/engine/ScriptEngineState.h
        ${CMAKE_SOURCE_DIR}/src/scripting/engine/ScriptEventBus.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/engine/ScriptEventBus.h
        ${CMAKE_SOURCE_DIR}/src/scripting/engine/ScriptingBootstrap.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/engine/ScriptingBootstrap.h
        ${CMAKE_SOURCE_DIR}/src/scripting/qt/ScriptQtHost.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/qt/ScriptQtHost.h
        ${CMAKE_SOURCE_DIR}/src/scripting/binding/ScriptBindings.cc
        ${CMAKE_SOURCE_DIR}/src/scripting/binding/ScriptBindings.h
)

add_executable(edm-scripting-smoke ${SCRIPTING_TEST_SOURCES})

target_include_directories(edm-scripting-smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/scripting
)

target_compile_definitions(edm-scripting-smoke PRIVATE
        UNICODE
        _UNICODE
)

if (MSVC)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(edm-scripting-smoke PRIVATE /utf-8 /W4 /sdl -Wno-unused-command-line-argument)
    else()
        target_compile_options(edm-scripting-smoke PRIVATE /utf-8 /W4 /sdl)
    endif()
    set_property(TARGET edm-scripting-smoke PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(edm-scripting-smoke PRIVATE -Wall -Wextra)
endif()

target_link_libraries(edm-scripting-smoke PRIVATE
        Qt6::Core
        src-downloader
        edm-common
        magic_enum::magic_enum
        SQLiteCpp
        jspp
)
