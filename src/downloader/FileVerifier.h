#pragma once
#include "expected.h"

#include <cstdint>
#include <optional>
#include <string>

namespace edm::downloader {

struct FileVerification {
    bool        ok{true};
    std::string message;
};

class FileVerifier {
public:
    [[nodiscard]] static Expected<std::string> md5Hex(std::string const& path);
    [[nodiscard]] static Expected<FileVerification>
    verify(std::string const& path, int64_t expectedSize, std::optional<std::string> const& expectedMd5);
};

} // namespace edm::downloader
