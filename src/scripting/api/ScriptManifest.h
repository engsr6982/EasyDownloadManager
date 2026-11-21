#pragma once

#include "expected.h"

#include <filesystem>
#include <optional>
#include <string>

namespace edm::script {

/// 脚本类型常量
inline constexpr auto kScriptType = "edm-script";

/// 脚本扩展描述文件（<scriptsDir>/<name>/manifest.json）。
///
/// 必填字段：name / type / entry / version；
/// 可选字段：author / description / homepage / icon。
struct ScriptManifest {
    std::string                name;    // 必须与目录名一致，字符集 [A-Za-z0-9._-]
    std::string                type;    // 固定为 "edm-script"
    std::string                entry;   // 入口 ES module，相对脚本目录的路径
    std::string                version; // 语义化版本字符串（major.minor.patch[-prerelease]）
    std::optional<std::string> author;
    std::optional<std::string> description;
    std::optional<std::string> homepage;
    std::optional<std::string> icon; // 卡片显示图标（相对脚本目录），缺失/不可读时 UI 回退默认图标
};

/// 读取并校验 dir/manifest.json。
/// 校验规则：必填字段齐全、name 与目录名一致、type 为 "edm-script"、
/// entry 文件存在、version 为合法 semver。
/// @return 通过校验的 manifest；失败返回错误说明。
Expected<ScriptManifest> loadManifest(std::filesystem::path const& dir);

} // namespace edm::script
