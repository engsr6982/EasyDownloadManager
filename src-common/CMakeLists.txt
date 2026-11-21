# 公共层：跨层通用代码（Qt-free）
add_library(edm-common STATIC
        CurlEx.cc
        CurlEx.h
)
add_library(edm::common ALIAS edm-common)

target_include_directories(edm-common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(edm-common INTERFACE
        NOMINMAX
        CURL_STATICLIB
)

target_link_libraries(edm-common PUBLIC
        fmt::fmt
        CURL::libcurl
)
