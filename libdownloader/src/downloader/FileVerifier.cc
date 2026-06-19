#include "FileVerifier.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#endif

namespace edm::downloader {

namespace {

std::string toLower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string bytesToHex(unsigned char const* data, unsigned int len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string base64Encode(unsigned char const* data, unsigned int len) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string           out;
    out.reserve(((len + 2) / 3) * 4);

    for (unsigned int i = 0; i < len; i += 3) {
        unsigned int octetA = data[i];
        unsigned int octetB = i + 1 < len ? data[i + 1] : 0;
        unsigned int octetC = i + 2 < len ? data[i + 2] : 0;
        unsigned int triple = (octetA << 16) | (octetB << 8) | octetC;

        out.push_back(table[(triple >> 18) & 0x3F]);
        out.push_back(table[(triple >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? table[(triple >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? table[triple & 0x3F] : '=');
    }

    return out;
}

} // namespace

Expected<std::string> FileVerifier::md5Hex(std::string const& path) {
#ifndef _WIN32
    (void)path;
    return makeStringError("MD5 verification is not implemented on this platform yet");
#else
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return makeStringError("Failed to open file for checksum: " + path);
    }

    BCRYPT_ALG_HANDLE  algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash      = nullptr;
    DWORD              objectLen = 0;
    DWORD              hashLen   = 0;
    DWORD              cbData    = 0;

    auto status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0);
    if (status < 0) return makeStringError("Failed to open MD5 provider");

    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLen), sizeof(objectLen), &cbData, 0);
    if (status >= 0) {
        status = BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(hashLen), &cbData, 0);
    }
    if (status < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return makeStringError("Failed to query MD5 provider properties");
    }

    std::vector<unsigned char> hashObject(objectLen);
    std::vector<unsigned char> digest(hashLen);

    status = BCryptCreateHash(algorithm, &hash, hashObject.data(), objectLen, nullptr, 0, 0);
    if (status < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return makeStringError("Failed to create MD5 hash");
    }

    std::array<char, 1024 * 1024> buffer{};
    while (file.good()) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        auto readBytes = file.gcount();
        if (readBytes > 0 &&
            BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(readBytes), 0) < 0) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return makeStringError("Failed to update MD5 digest");
        }
    }

    if (BCryptFinishHash(hash, digest.data(), hashLen, 0) < 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return makeStringError("Failed to finalize MD5 digest");
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return bytesToHex(digest.data(), hashLen);
#endif
}

Expected<FileVerification>
FileVerifier::verify(std::string const& path, int64_t expectedSize, std::optional<std::string> const& expectedMd5) {
    std::error_code ec;
    auto            actualSize = static_cast<int64_t>(std::filesystem::file_size(path, ec));
    if (ec) {
        return makeStringError("Failed to stat downloaded file: " + ec.message());
    }

    if (expectedSize >= 0 && actualSize != expectedSize) {
        return FileVerification{false, "Downloaded file size does not match metadata"};
    }

    if (!expectedMd5 || expectedMd5->empty()) {
        return FileVerification{true, {}};
    }

    auto md5Res = md5Hex(path);
    if (!md5Res) {
        return forwardError(md5Res.error());
    }

    auto expected = *expectedMd5;
    expected.erase(std::remove_if(expected.begin(), expected.end(), [](unsigned char c) {
                       return std::isspace(c) != 0 || c == '"' || c == '\'';
                   }),
                   expected.end());

    auto actualHex = toLower(md5Res.value());
    if (expected.size() == 32) {
        return FileVerification{toLower(expected) == actualHex, "MD5 checksum mismatch"};
    }

    std::vector<unsigned char> bytes;
    bytes.reserve(16);
    for (size_t i = 0; i + 1 < actualHex.size(); i += 2) {
        bytes.push_back(static_cast<unsigned char>(std::stoul(actualHex.substr(i, 2), nullptr, 16)));
    }
    return FileVerification{expected == base64Encode(bytes.data(), static_cast<unsigned int>(bytes.size())),
                            "MD5 checksum mismatch"};
}

} // namespace edm::downloader
