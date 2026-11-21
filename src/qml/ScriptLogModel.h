#pragma once

#include "scripting/api/ScriptConsole.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace edm {

/// 虚拟控制台的 QML 数据模型：包装脚本核心层的 ScriptConsole 环形缓冲，
/// 经其变更回调做增量行更新。
class ScriptLogModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ScriptLogModel is created by the scripting subsystem")

public:
    enum Level {
        Debug = static_cast<int>(script::LogLevel::Debug),
        Info  = static_cast<int>(script::LogLevel::Info),
        Warn  = static_cast<int>(script::LogLevel::Warn),
        Error = static_cast<int>(script::LogLevel::Error),
    };
    Q_ENUM(Level)

    explicit ScriptLogModel(std::shared_ptr<script::ScriptConsole> console, QObject* parent = nullptr);
    ~ScriptLogModel() override;

    int                    rowCount(QModelIndex const& parent = {}) const override;
    QVariant               data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();

private:
    void onConsoleChanged();

    std::shared_ptr<script::ScriptConsole> console_;
    int                                    knownCount_{0};
};

} // namespace edm
