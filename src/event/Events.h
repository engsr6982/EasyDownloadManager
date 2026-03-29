#pragma once
#include <QString>
#include <optional>
#include <string>
#include <QMetaType>

#include "fmt/format.h"

namespace edm {

struct MetaInfoResultEvent {
    QString url;
    bool    success;
    qint64  fileSize;
    bool    supportRange;
    QString errorMessage;

    inline std::string toDebugString() const {
        return fmt::format(
            "url: {}, success: {}, fileSize: {}, supportRange: {}, errorMessage: {}",
            url.toStdString(),
            success,
            fileSize,
            supportRange,
            errorMessage.toStdString()
        );
    }
};

} // namespace edm

Q_DECLARE_METATYPE(edm::MetaInfoResultEvent)

