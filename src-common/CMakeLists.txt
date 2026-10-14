# 仅头文件公共层：跨层通用代码（Qt-free，只依赖 std + fmt）
add_library(edm-common INTERFACE)
add_library(edm::common ALIAS edm-common)

target_include_directories(edm-common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(edm-common INTERFACE NOMINMAX)

target_link_libraries(edm-common INTERFACE fmt::fmt)