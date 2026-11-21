#pragma once

#include <optional>
#include <string>
#include <vector>

namespace edm::script {

/// 文件访问（文本读写）。
/// 路径契约：POSIX 风格为主，兼容 Windows 风格输入；相对路径由绑定层以脚本目录为根解析。
/// 布尔返回值表示操作是否成功；OS 级拒绝（权限等）原样抛出异常。
struct ScriptFs {
    static bool exists(std::string const& path);
    /// 非普通文件（不存在、目录等）统一返回 nullopt
    static std::optional<std::string> readFile(std::string const& path);
    /// 自动创建缺失的父目录
    static bool writeFile(std::string const& path, std::string const& content);
    /// 拷贝文件或目录树，覆盖同名
    static bool copy(std::string const& src, std::string const& dst);
    /// 同卷 rename，跨卷回退为拷贝后删除
    static bool move(std::string const& src, std::string const& dst);
    /// 仅创建单级目录，父目录必须存在
    static bool mkdir(std::string const& path);
    /// 创建目录并补齐缺失的父目录
    static bool mkdir(std::string const& path, bool recursive);
    /// 追加写入；createIfNotExists=false 时文件不存在则返回 false
    static bool appendFile(std::string const& path, std::string const& content, bool createIfNotExists);
    static bool appendFile(std::string const& path, std::string const& content);
    /// 始终递归删除 @returns 是否有条目被删除
    static bool remove(std::string const& path);
    static bool isDirectory(std::string const& path);
    static bool isFile(std::string const& path);
};

/// POSIX 路径工具（对齐 Node.js path.posix；'\' 视作分隔符并归一化为 '/'）
struct ScriptPath {
    /// 以 '/' 连接非空段并归一化（全空返回 "."）
    static std::string join(std::vector<std::string> const& parts);
    static std::string basename(std::string const& path);
    static std::string dirname(std::string const& path);
    static std::string extname(std::string const& path);
    /// from 到 to 的相对路径
    static std::string relative(std::string const& root, std::string const& from, std::string const& to);
    /// 以脚本目录为根解析（绝对路径归一化后原样返回）
    static std::string absolute(std::string const& root, std::string const& path);
    static std::string normalize(std::string const& path);
};

} // namespace edm::script
