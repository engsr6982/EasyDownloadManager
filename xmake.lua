add_rules("mode.release", "mode.debug")

add_requires("magic_enum 0.9.7")
add_requires("libcurl 8.11.0", {
    configs = {
        shared = false,
        openssl3 = true,
        zlib = true,
        nghttp2 = false
    }
});

if is_plat("windows") then
    if not has_config("vs_runtime") then
        if is_mode("debug") then
            set_runtimes("MDd")
        else
            set_runtimes("MD")
        end
    end
end

target("EasyDownloadManager")
    add_rules("qt.widgetapp")
    add_cxflags(
        "/utf-8", "/W4", "/sdl"
    )
    add_defines(
        "NOMINMAX",
        "UNICODE"
    )
    set_symbols("debug")

    add_files(
        "src/**.cc",
        "src/**.cpp",
        "src/**.h",
        "src/**.ui",
        "src/**.qrc",
        "src/**.rc"
    )
    add_includedirs("src")
    add_headerfiles("src/*.h")

    if is_mode("debug") then
        add_defines("EDM_DEBUG")
    end

    add_packages("magic_enum", "libcurl")