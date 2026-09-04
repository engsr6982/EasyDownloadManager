#pragma once
#include "Global.h"
#include "downloader/DownloadTypes.h"
#include "expected.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>


#include <glaze/beve/write.hpp>
#include <glaze/glaze.hpp>
#include <glaze/beve/lazy.hpp>
#include <glaze/core/context.hpp>

#include <fmt/format.h>



namespace edm ::schema {

struct ChecksumHasher {
    static constexpr uint64_t kPrime  = 1099511628211ULL;
    static constexpr uint64_t kOffset = 1469598103934665603ULL;

    constexpr uint64_t operator()(std::byte const* bytes, size_t len) const {
        uint64_t hash = kOffset;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(bytes[i]);
            hash *= kPrime;
        }
        return hash;
    }
};


// memory layout:
// | magic (raw 4 bytes) | version (raw 4 bytes) | checksum (raw 8 bytes) | <glaze beve payload> |
//
// About cross-platform:
// - magic, version, checksum are raw 4/8 bytes, so they are always little-endian
//   on all platforms

struct IEnvelope {
    template <typename T>
        requires std::is_integral_v<T>
    static void write_integral(T value, std::vector<std::byte>& bytes) {
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        auto buffer = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        bytes.insert(bytes.end(), buffer.begin(), buffer.end());
    }

    template <typename T>
        requires std::is_integral_v<T>
    static T read_integral(std::vector<std::byte> const& bytes, size_t offset) {
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
        if constexpr (std::endian::native == std::endian::big && sizeof(T) > 1) {
            value = std::byteswap(value);
        }
        return value;
    }

    template <typename T>
        requires std::is_integral_v<T>
    static bool write_integral(std::ofstream& ofs, T value) {
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        ofs.write(reinterpret_cast<char const*>(&value), sizeof(T));
        return ofs.good();
    }

    template <typename T>
        requires std::is_integral_v<T>
    static bool read_integral(std::ifstream& ifs, T& value) {
        ifs.read(reinterpret_cast<char*>(&value), sizeof(T));
        if constexpr (std::endian::native == std::endian::big && sizeof(T) > 1) {
            value = std::byteswap(value);
        }
        return ifs.good();
    }
};


template <typename P>
    requires std::is_aggregate_v<P>
class EnvelopeWriter : IEnvelope {
    uint32_t const              magic_;
    uint32_t const              version_;
    std::filesystem::path const path_;
    std::ofstream               ofs_;
    P                           payload_{};

    void tryOpenFile() {
        ofs_.open(path_, std::ios::binary | std::ios::trunc);
        if (!ofs_.is_open()) {
            throw std::runtime_error("Failed to open file for writing");
        }
    }

public:
    explicit EnvelopeWriter(uint32_t magic, uint32_t version, std::filesystem::path path)
    : magic_(magic),
      version_(version),
      path_(std::move(path)) {
        tryOpenFile();
    }
    explicit EnvelopeWriter(uint32_t magic, uint32_t version, P payload, std::filesystem::path path)
    : magic_(magic),
      version_(version),
      payload_(std::move(payload)),
      path_(std::move(path)) {
        tryOpenFile();
    }

    inline void setPayload(P payload) { this->payload_ = std::move(payload); }

    inline P& getPayload() { return payload_; }

    inline P*       operator->() { return &payload_; }
    inline P const* operator->() const { return &payload_; }

    inline Expected<> flush() {
        if (!ofs_.is_open()) {
            return makeStringError("File is not open");
        }

        std::vector<std::byte> payload_buf;
        if (glz::error_ctx error = glz::write_beve(payload_, payload_buf)) {
            return makeStringError(
                fmt::format(
                    "Failed to write envelope: {}, code: {}, processed: {}",
                    error.custom_error_message,
                    (uint32_t)error.ec,
                    error.count
                )
            );
        }

        uint64_t checksum_ = ChecksumHasher{}(payload_buf.data(), payload_buf.size());

        if (!write_integral(ofs_, magic_)) {
            return makeStringError("Failed to write magic");
        }
        if (!write_integral(ofs_, version_)) {
            return makeStringError("Failed to write version");
        }
        if (!write_integral(ofs_, checksum_)) {
            return makeStringError("Failed to write checksum");
        }
        ofs_.write(reinterpret_cast<char const*>(payload_buf.data()), payload_buf.size());
        if (!ofs_.good()) {
            return makeStringError("Failed to write payload");
        }
        ofs_.flush();
        if (!ofs_.good()) {
            return makeStringError("Failed to write payload");
        }
        return {};
    }
};

class EnvelopeReader : IEnvelope {
    uint32_t const              magic_;
    uint32_t const              version_;
    std::filesystem::path const path_;

public:
    explicit EnvelopeReader(uint32_t magic, uint32_t version, std::filesystem::path path)
    : magic_(magic),
      version_(version),
      path_(std::move(path)) {}

    template <typename T>
        requires std::is_aggregate_v<T>
    Expected<T> read(bool allowVersionMismatch = false) {
        std::ifstream ifs(path_, std::ios::binary);
        if (!ifs.is_open()) {
            return makeStringError("Failed to open file for reading");
        }

        uint32_t magic;
        uint32_t version;
        uint64_t checksum;
        if (!read_integral(ifs, magic) || !read_integral(ifs, version) || !read_integral(ifs, checksum)) {
            return makeStringError("Failed to read envelope header");
        }

        if (magic != magic_) {
            return makeStringError(fmt::format("Invalid magic: expected {}, got {}", magic_, magic));
        }
        if (!allowVersionMismatch && version != version_) {
            return makeStringError(fmt::format("Invalid version: expected {}, got {}", version_, version));
        }

        std::streampos pos = ifs.tellg(); // current position (should be at the end of the header)
        ifs.seekg(0, std::ios::end);
        std::streampos end = ifs.tellg(); // end of file
        ifs.seekg(pos);

        size_t                 payload_size = static_cast<size_t>(end - pos);
        std::vector<std::byte> payload_buf(payload_size);

        if (payload_size > 0) {
            ifs.read(reinterpret_cast<char*>(payload_buf.data()), payload_size);
            if (!ifs.good()) {
                return makeStringError("Failed to read payload");
            }
        }

        uint64_t checksum_ = ChecksumHasher{}(payload_buf.data(), payload_buf.size());
        if (checksum != checksum_) {
            return makeStringError(fmt::format("Invalid checksum: expected {}, got {}", checksum, checksum_));
        }

        T payload;
        if (glz::error_ctx error = glz::read_beve(payload, payload_buf)) {
            return makeStringError(
                fmt::format(
                    "Failed to read envelope: {}, code: {}, processed: {}",
                    error.custom_error_message,
                    (uint32_t)error.ec,
                    error.count
                )
            );
        }
        return payload;
    }
};


} // namespace edm::schema


namespace edm ::schema {

struct RangeRecord {
    int64_t start;
    int64_t end;
    int64_t downloaded;
    bool    completed;
};

struct MetaPaylad {
    int64_t                  fileSize;
    uint64_t                 rangeCount;
    std::vector<RangeRecord> ranges;
};

} // namespace edm::schema