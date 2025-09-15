#pragma once
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <stdexcept>


namespace edm::enum_utils {

/**
 * 从枚举值获取枚举下标
 * @tparam Enum 枚举
 * @param value 值
 * @return 下标
 */
template <typename Enum>
constexpr std::optional<int> getIndexFromEnumValue(Enum value) {
    static_assert(std::is_enum_v<Enum>, "Enum must be an enum type");
    constexpr auto values = magic_enum::enum_values<Enum>();
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] == value) return static_cast<int>(i);
    }
    return std::nullopt;
}

/**
 * 从下标获取枚举
 * @tparam Enum 枚举
 * @param idx 下标
 * @return 枚举值
 */
template <typename Enum>
constexpr Enum getEnumFromIndex(int idx) {
    // from index get enum
    static_assert(std::is_enum_v<Enum>, "Enum must be an enum type");
    constexpr auto values = magic_enum::enum_values<Enum>();
    if (idx < 0 || idx >= static_cast<int>(values.size())) {
        throw std::out_of_range("getEnumFromIndex: index out of range");
    }
    return values[idx];
}

/**
 * 从枚举值获取枚举下标
 * @tparam Enum 枚举值
 * @param value 枚举值 (static_cast<int>(Enum::foo))
 * @return 下标
 */
template <typename Enum>
constexpr std::optional<int> getIndexFromRawValue(int value) {
    static_assert(std::is_enum_v<Enum>, "Enum must be an enum type");
    constexpr auto values = magic_enum::enum_values<Enum>();
    for (size_t i = 0; i < values.size(); ++i) {
        if (static_cast<int>(values[i]) == value) return static_cast<int>(i);
    }
    return std::nullopt;
}

/**
 * 从下标获取枚举原始值
 * @tparam Enum 枚举
 * @param idx 下标
 * @return 枚举原始值 static_cast<int>
 */
template <typename Enum>
constexpr auto getRawValueFromIndex(int idx) {
    return static_cast<int>(getEnumFromIndex<Enum>(idx));
}

} // namespace edm::enum_utils