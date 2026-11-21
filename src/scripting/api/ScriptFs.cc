#include "ScriptFs.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace edm::script {

namespace {

namespace fs = std::filesystem;

inline std::string normalizeSeparators(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

inline fs::path nativePath(std::string const& path) { return fs::path{normalizeSeparators(path)}; }

inline void createParentDirs(fs::path const& p) {
    auto const parent = p.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
    }
}

inline std::string toPOSIXPath(fs::path const& p) {
    auto        u8 = p.generic_u8string();
    std::string s(reinterpret_cast<char const*>(u8.data()), u8.size());
    return s;
}

// 移植自 Node.js v22 lib/path.js normalizeString()
std::string normalizeString(std::string_view path, bool allowAboveRoot) {
    std::string res;
    std::size_t lastSegmentLength = 0;
    ptrdiff_t   lastSlash         = -1;
    int         dots              = 0;
    char        code              = 0;

    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i < path.size()) {
            code = path[i];
        } else if (code == '/') {
            break;
        } else {
            code = '/';
        }

        if (code == '/') {
            if (lastSlash == static_cast<ptrdiff_t>(i) - 1 || dots == 1) {
                // 空段或 "." 段：跳过
            } else if (dots == 2) {
                if (res.size() < 2 || lastSegmentLength != 2 || res[res.size() - 1] != '.'
                    || res[res.size() - 2] != '.') {
                    if (res.size() > 2) {
                        auto const lastSlashIndex =
                            static_cast<ptrdiff_t>(res.size()) - static_cast<ptrdiff_t>(lastSegmentLength) - 1;
                        if (lastSlashIndex == -1) {
                            res.clear();
                            lastSegmentLength = 0;
                        } else {
                            res.resize(static_cast<std::size_t>(lastSlashIndex));
                            auto const lastSep = res.find_last_of('/');
                            lastSegmentLength  = (lastSep == std::string::npos) ? res.size() : res.size() - 1 - lastSep;
                        }
                        lastSlash = static_cast<ptrdiff_t>(i);
                        dots      = 0;
                        continue;
                    } else if (!res.empty()) {
                        res.clear();
                        lastSegmentLength = 0;
                        lastSlash         = static_cast<ptrdiff_t>(i);
                        dots              = 0;
                        continue;
                    }
                }
                if (allowAboveRoot) {
                    res               += res.empty() ? ".." : "/..";
                    lastSegmentLength  = 2;
                }
            } else {
                if (!res.empty()) {
                    res += '/';
                }
                res.append(
                    path.substr(static_cast<std::size_t>(lastSlash) + 1, i - static_cast<std::size_t>(lastSlash) - 1)
                );
                lastSegmentLength = i - static_cast<std::size_t>(lastSlash) - 1;
            }
            lastSlash = static_cast<ptrdiff_t>(i);
            dots      = 0;
        } else if (code == '.' && dots != -1) {
            ++dots;
        } else {
            dots = -1;
        }
    }
    return res;
}

inline fs::path absoluteNormalized(std::string const& root, std::string const& path) {
    auto p = fs::absolute(nativePath(path)).lexically_normal();
    if (path.empty() || !fs::path{normalizeSeparators(path)}.is_absolute()) {
        p = fs::absolute(nativePath(root) / nativePath(path)).lexically_normal();
    }
    return p;
}

} // namespace

bool ScriptFs::exists(std::string const& path) { return fs::exists(nativePath(path)); }

std::optional<std::string> ScriptFs::readFile(std::string const& path) {
    fs::path const p = nativePath(path);
    if (!fs::is_regular_file(p)) {
        return std::nullopt;
    }
    std::ifstream in(p); // 文本模式
    if (!in) {
        return std::nullopt;
    }
    std::string const data{(std::istreambuf_iterator<char>(in)), {}};
    if (in.bad()) {
        return std::nullopt;
    }
    return data;
}

bool ScriptFs::writeFile(std::string const& path, std::string const& content) {
    fs::path const p = nativePath(path);
    createParentDirs(p);
    std::ofstream out(p); // 文本模式
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.flush();
    return out.good();
}

bool ScriptFs::copy(std::string const& src, std::string const& dst) {
    fs::path const srcP = nativePath(src);
    fs::path const dstP = nativePath(dst);
    createParentDirs(dstP);
    if (fs::is_directory(srcP)) {
        fs::copy(srcP, dstP, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        return true;
    }
    return fs::copy_file(srcP, dstP, fs::copy_options::overwrite_existing);
}

bool ScriptFs::move(std::string const& src, std::string const& dst) {
    fs::path const srcP = nativePath(src);
    fs::path const dstP = nativePath(dst);
    createParentDirs(dstP);
    try {
        fs::rename(srcP, dstP);
        return true;
    } catch (fs::filesystem_error const& e) {
        if (e.code() != std::errc::cross_device_link) {
            throw; // 操作系统拒绝（权限等），原样抛给脚本
        }
    }
    fs::copy(srcP, dstP, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    fs::remove_all(srcP);
    return true;
}

bool ScriptFs::mkdir(std::string const& path) { return fs::create_directory(nativePath(path)); }

bool ScriptFs::mkdir(std::string const& path, bool recursive) {
    if (recursive) {
        return fs::create_directories(nativePath(path));
    }
    return mkdir(path);
}

bool ScriptFs::appendFile(std::string const& path, std::string const& content, bool createIfNotExists) {
    fs::path const p = nativePath(path);
    if (!createIfNotExists && !fs::exists(p)) {
        return false;
    }
    if (createIfNotExists) {
        createParentDirs(p);
    }
    std::ofstream out(p, std::ios::binary | std::ios::app);
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.flush();
    return out.good();
}

bool ScriptFs::appendFile(std::string const& path, std::string const& content) {
    return appendFile(path, content, false);
}

bool ScriptFs::remove(std::string const& path) { return fs::remove_all(nativePath(path)) > 0; }

bool ScriptFs::isDirectory(std::string const& path) { return fs::is_directory(nativePath(path)); }

bool ScriptFs::isFile(std::string const& path) { return fs::is_regular_file(nativePath(path)); }

std::string ScriptPath::join(std::vector<std::string> const& parts) {
    std::vector<std::string> segments;
    segments.reserve(parts.size());
    for (auto& part : parts) {
        auto raw = normalizeSeparators(part);
        if (!raw.empty()) segments.push_back(std::move(raw));
    }
    if (segments.empty()) {
        return ".";
    }
    std::string joined = segments.front();
    for (std::size_t i = 1; i < segments.size(); ++i) {
        joined += '/';
        joined += segments[i];
    }
    return normalize(joined);
}

std::string ScriptPath::normalize(std::string const& path) {
    std::string const normalized = normalizeSeparators(path);
    if (normalized.empty()) {
        return ".";
    }
    bool const isAbsolute        = normalized.front() == '/';
    bool const trailingSeparator = normalized.back() == '/';

    std::string res = normalizeString(normalized, !isAbsolute);
    if (res.empty()) {
        if (isAbsolute) return "/";
        return trailingSeparator ? "./" : ".";
    }
    if (trailingSeparator) {
        res += '/';
    }
    return isAbsolute ? "/" + res : res;
}

std::string ScriptPath::basename(std::string const& path) {
    std::string const normalized = normalizeSeparators(path);
    std::string_view  v{normalized};
    std::size_t       end = v.size();
    while (end > 0 && v[end - 1] == '/') {
        --end;
    }
    if (end == 0) {
        return {};
    }
    auto const start = v.rfind('/', end - 1);
    if (start == std::string_view::npos) {
        return std::string(v.substr(0, end));
    }
    return std::string(v.substr(start + 1, end - start - 1));
}

std::string ScriptPath::dirname(std::string const& path) {
    std::string const normalized = normalizeSeparators(path);
    std::string_view  v{normalized};
    if (v.empty()) {
        return ".";
    }
    bool const hasRoot = v.front() == '/';
    if (v.size() == 1) {
        return hasRoot ? std::string("/") : std::string(".");
    }

    std::size_t end          = std::string_view::npos;
    bool        matchedSlash = true;
    for (std::size_t i = v.size() - 1; i >= 1; --i) {
        if (v[i] == '/') {
            if (!matchedSlash) {
                end = i;
                break;
            }
        } else {
            matchedSlash = false;
        }
    }
    if (end == std::string_view::npos) {
        return hasRoot ? std::string("/") : std::string(".");
    }
    if (hasRoot && end == 1) {
        return "//";
    }
    return std::string(v.substr(0, end));
}

std::string ScriptPath::extname(std::string const& path) {
    std::string const normalized = normalizeSeparators(path);
    std::string_view  v{normalized};
    ptrdiff_t         startPart    = 0;
    ptrdiff_t         startDot     = -1;
    ptrdiff_t         end          = -1;
    bool              matchedSlash = true;
    int               preDotState  = 0;

    for (ptrdiff_t i = static_cast<ptrdiff_t>(v.size()) - 1; i >= 0; --i) {
        char const c = v[static_cast<std::size_t>(i)];
        if (c == '/') {
            if (!matchedSlash) {
                startPart = i + 1;
                break;
            }
            continue;
        }
        if (end == -1) {
            matchedSlash = false;
            end          = i + 1;
        }
        if (c == '.') {
            if (startDot == -1) {
                startDot = i;
            } else if (preDotState != 1) {
                preDotState = 1;
            }
        } else if (startDot != -1) {
            preDotState = -1;
        }
    }

    if (startDot == -1 || end == -1 || preDotState == 0
        || (preDotState == 1 && startDot == end - 1 && startDot == startPart + 1)) {
        return {};
    }
    return std::string(v.substr(static_cast<std::size_t>(startDot), static_cast<std::size_t>(end - startDot)));
}

std::string ScriptPath::relative(std::string const& root, std::string const& from, std::string const& to) {
    (void)root;
    auto const absFrom = fs::absolute(nativePath(from)).lexically_normal();
    auto const absTo   = fs::absolute(nativePath(to)).lexically_normal();
    return toPOSIXPath(absTo.lexically_relative(absFrom));
}

std::string ScriptPath::absolute(std::string const& root, std::string const& path) {
    auto const p          = absoluteNormalized(root, path);
    auto       s          = toPOSIXPath(p);
    bool const isRootForm = (s == "/") || (s == "//") || (s.size() == 3 && s[1] == ':' && s[2] == '/');
    if (!isRootForm && !s.empty() && s.back() == '/') {
        s.pop_back();
    }
    return s;
}

} // namespace edm::script
