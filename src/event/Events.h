#pragma once
#include <QMetaType>
#include <QString>
#include <optional>
#include <string>

#include "fmt/format.h"

#include "downloader/FetchedMetaInfo.h"

namespace edm {

struct MetaInfoResultEvent {
    // info or error
    std::variant<std::monostate, downloader::FetchedMetaInfo, std::string> result{};

    template <typename T>
    inline bool hold() const {
        return std::holds_alternative<T>(result);
    }
};

} // namespace edm

Q_DECLARE_METATYPE(edm::MetaInfoResultEvent)
