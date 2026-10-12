#pragma once
#include <any>
#include <expected>
#include <memory>
#include <string>
#include <utility>


namespace edm {


struct Error {
    using MessageProvider = std::string (*)(std::any const& data);
    MessageProvider message_provider;
    std::any        data;

    inline std::string message() const { return message_provider(data); }

    template <typename T>
    decltype(auto) cast() const {
        return std::any_cast<T>(data);
    }
};

template <class T = void>
using Expected   = ::std::expected<T, Error>;
using Unexpected = ::std::unexpected<Error>;


inline Unexpected forwardError(Error& err) noexcept { return ::std::unexpected(std::move(err)); }

template <class... Args>
inline Unexpected makeError(Args&&... args) noexcept {
    return std::unexpected<Error>(std::in_place, std::forward<Args>(args)...);
}
inline Unexpected makeStringError(std::string str) noexcept {
    return makeError([](std::any const& data) { return std::any_cast<std::string const&>(data); }, str);
}

} // namespace edm
