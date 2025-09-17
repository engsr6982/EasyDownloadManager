add_rules("mode.release", "mode.debug")

add_requires("magic_enum 0.9.7")
add_requires("sqlite_orm 1.9.1")
LibCurlPackage = "libcurl 8.11.0";

if is_plat("windows") then
    add_requires(LibCurlPackage, {
        configs = {
            shared = false,
            openssl = false, -- use Schannel
            openssl3 = false,
            zlib = true,
            nghttp2 = false
        }
    });
    if not has_config("vs_runtime") then
        if is_mode("debug") then
            set_runtimes("MDd")
        else
            set_runtimes("MD")
        end
    end
else
    add_requires(LibCurlPackage, {
        configs = {
            shared = false,
            openssl = false,
            openssl3 = true,
            zlib = true,
            nghttp2 = false
        }
    });
end

target("EasyDownloadManager")
    set_languages("cxx20")
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

    add_packages("magic_enum", "libcurl", "sqlite_orm")