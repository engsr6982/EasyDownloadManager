#pragma once
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <stdexcept>


namespace edm::enum_utils {

template <typename Enum>
constexpr std::optional<int> enum_index(Enum value) {
    static_assert(std::is_enum_v<Enum>, "Enum must be an enum type");
    constexpr auto values = magic_enum::enum_values<Enum>();
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] == value) return static_cast<int>(i);
    }
    return std::nullopt;
}

template <typename Enum>
constexpr Enum enum_from_index(int idx) {
    static_assert(std::is_enum_v<Enum>, "Enum must be an enum type");
    constexpr auto values = magic_enum::enum_values<Enum>();
    if (idx < 0 || idx >= static_cast<int>(values.size())) {
        throw std::out_of_range("enum_from_index: index out of range");
    }
    return values[idx];
}

template <typename Enum>
constexpr std::optional<int> enum_value_index(int int_value) {
    static_assert(std::is_enum_v<Enum>, "Enum must be an enum type");
    constexpr auto values = magic_enum::enum_values<Enum>();
    for (size_t i = 0; i < values.size(); ++i) {
        if (static_cast<int>(values[i]) == int_value) return static_cast<int>(i);
    }
    return std::nullopt;
}

} // namespace edm::enum_utils