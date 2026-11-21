#include "ScriptManifest.h"

#include <fmt/format.h>

#include <glaze/glaze.hpp>

#include <cctype>
#include <fstream>
#include <sstream>

namespace edm::script {

namespace {

struct ManifestRaw {
    // 全部用 optional 承载，以便区分"字段缺失"与"空字符串"
    std::optional<std::string> name;
    std::optional<std::string> type;
    std::optional<std::string> entry;
    std::optional<std::string> version;
    std::optional<std::string> author;
    std::optional<std::string> description;
    std::optional<std::string> homepage;
    std::optional<std::string> icon;
};

bool isValidName(std::string const& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_' && c != '-' && c != '.') return false;
    }
    return name != "." && name != "..";
}

bool isSemver(std::string_view v) {
    if (v.empty() || !std::isdigit(static_cast<unsigned char>(v.front()))) return false;
    for (char const delim : {'-', '+'}) {
        if (auto pos = v.find(delim); pos != std::string_view::npos) v = v.substr(0, pos);
    }
    int  section  = 0;
    bool hasDigit = false;
    for (char c : v) {
        if (c == '.') {
            if (!hasDigit) return false;
            hasDigit = false;
            ++section;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            hasDigit = true;
        } else {
            return false;
        }
    }
    return hasDigit && section >= 1; // major.minor[.patch]
}

} // namespace

template <>
struct glz::meta<ManifestRaw> {
    using T                     = ManifestRaw;
    static constexpr auto value = object(
        "name",
        &T::name,
        "type",
        &T::type,
        "entry",
        &T::entry,
        "version",
        &T::version,
        "author",
        &T::author,
        "description",
        &T::description,
        "homepage",
        &T::homepage,
        "icon",
        &T::icon
    );
};

Expected<ScriptManifest> loadManifest(std::filesystem::path const& dir) {
    auto const    manifestPath = dir / "manifest.json";
    std::ifstream file(manifestPath, std::ios::binary);
    if (!file) {
        return makeStringError("manifest.json not found in " + dir.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto const content = buffer.str();

    ManifestRaw raw;
    // 容忍未知字段：manifest 可携带 "$schema" 等仅面向 IDE 的键
    auto const ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(raw, content);
    if (ec) {
        return makeStringError("failed to parse manifest.json: " + std::string(glz::format_error(ec, content)));
    }

    ScriptManifest manifest;
    if (!raw.name || !raw.type || !raw.entry || !raw.version) {
        return makeStringError("manifest.json missing required fields (name/type/entry/version)");
    }
    manifest.name        = std::move(*raw.name);
    manifest.type        = std::move(*raw.type);
    manifest.entry       = std::move(*raw.entry);
    manifest.version     = std::move(*raw.version);
    manifest.author      = std::move(raw.author);
    manifest.description = std::move(raw.description);
    manifest.homepage    = std::move(raw.homepage);
    manifest.icon        = std::move(raw.icon);

    auto const folderName = dir.filename().string();
    if (!isValidName(manifest.name)) {
        return makeStringError("invalid script name \"" + manifest.name + "\"");
    }
    if (manifest.name != folderName) {
        return makeStringError(
            fmt::format("manifest name \"{}\" does not match folder name \"{}\"", manifest.name, folderName)
        );
    }
    if (manifest.type != kScriptType) {
        return makeStringError(
            fmt::format("unsupported script type \"{}\" (expected \"{}\")", manifest.type, kScriptType)
        );
    }
    if (manifest.entry.empty() || manifest.entry.starts_with('/') || manifest.entry.contains("..")) {
        return makeStringError("invalid entry path \"" + manifest.entry + "\"");
    }
    if (!isSemver(manifest.version)) {
        return makeStringError("invalid version string \"" + manifest.version + "\" (expected semver)");
    }
    if (manifest.icon) {
        // 仅做路径安全校验；文件不存在时由展示层回退默认图标，不算加载错误
        if (manifest.icon->empty() || manifest.icon->starts_with('/') || manifest.icon->contains("..")
            || manifest.icon->contains('\\')) {
            return makeStringError("invalid icon path \"" + *manifest.icon + "\" (relative posix path expected)");
        }
    }

    std::error_code fsEc;
    if (!std::filesystem::is_regular_file(dir / std::filesystem::path(manifest.entry), fsEc)) {
        return makeStringError("entry file not found: " + manifest.entry);
    }

    return manifest;
}

} // namespace edm::script
