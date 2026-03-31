#pragma once

#include "nonstd/expected.hpp"

namespace edm {

class Error;

template <class T = void>
using Expected   = ::nonstd::expected<T, Error>;
using Unexpected = ::nonstd::unexpected_type<Error>;

struct IErrorInfo {
    IErrorInfo() noexcept                        = default;
    virtual ~IErrorInfo()                        = default;
    virtual std::string message() const noexcept = 0;
};

class Error {
    mutable std::unique_ptr<IErrorInfo> info{nullptr};

public:
    Error(Error const&) noexcept            = delete;
    Error& operator=(Error const&) noexcept = delete;

    Error(Error&&) noexcept            = default;
    Error& operator=(Error&&) noexcept = default;

    Error() noexcept  = default;
    ~Error() noexcept = default;

    Error(std::unique_ptr<IErrorInfo> e) noexcept : info(std::move(e)) {}
    Error(::nonstd::unexpected_type<Error> e) noexcept : Error(std::move(e.error())) {}

    std::string message() const noexcept try { return info ? info->message() : "success"; } catch (...) {
        return "unknown error";
    }

    operator bool() noexcept { return info != nullptr; }

    operator Unexpected() noexcept { return ::nonstd::make_unexpected<Error>(std::in_place, std::move(info)); }
};

struct StringError : IErrorInfo {
    std::string str;
    StringError(std::string str) : str(std::move(str)) {}
    std::string message() const noexcept override { return str; }
};

inline Unexpected forwardError(Error& err) noexcept { return ::nonstd::make_unexpected(std::move(err)); }

template <std::derived_from<IErrorInfo> T, class... Args>
inline Unexpected makeError(Args&&... args) noexcept {
    return ::nonstd::make_unexpected<Error>(std::in_place, std::make_unique<T>(std::forward<Args>(args)...));
}
inline Unexpected makeStringError(std::string str) noexcept { return makeError<StringError>(std::move(str)); }

} // namespace edm

namespace nonstd ::expected_lite {

template <>
class bad_expected_access<edm::Error> : public bad_expected_access<void> {
    std::shared_ptr<edm::Error> error_;
    std::string                 message_;

public:
    explicit bad_expected_access(edm::Error& e) noexcept
    : error_(::std::make_shared<edm::Error>(::std::move(e))),
      message_(error_->message()) {}

    char const* what() const noexcept override { return message_.c_str(); }

    edm::Error& error() & { return *error_; }

    edm::Error const& error() const& { return *error_; }

    edm::Error&& error() && { return ::std::move(*error_); }

    edm::Error const&& error() const&& { return ::std::move(*error_); }
};

template <>
struct error_traits<edm::Error> {
    static void rethrow(edm::Error const& e) { throw bad_expected_access<edm::Error>{const_cast<edm::Error&>(e)}; }
};

} // namespace nonstd::expected_lite