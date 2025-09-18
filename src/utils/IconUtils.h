#pragma once
#include <qfileiconprovider.h>


namespace edm::icon_utils {


struct EdmIconProvider {
    EdmIconProvider() = delete;

    enum class Type {
        Logo     = -1, // 程序Logo
        Computer = QFileIconProvider::IconType::Computer,
        Desktop  = QFileIconProvider::IconType::Desktop,
        Trashcan = QFileIconProvider::IconType::Trashcan,
        Network  = QFileIconProvider::IconType::Network,
        Drive    = QFileIconProvider::IconType::Drive,
        Folder   = QFileIconProvider::IconType::Folder,
        File     = QFileIconProvider::IconType::File,

        VideoFile      = 20, // 视频文件
        AudioFile      = 21, // 音频文件
        CompressedFile = 22, // 压缩文件
        DocumentFile   = 23, // 文档文件
        Application    = 24, // 应用程序
    };

    inline static bool isQtCanProvide(Type type) {
        if (type >= Type::Computer && type <= Type::File) {
            return true;
        }
        return false;
    }

    inline static std::optional<QFileIconProvider::IconType> toQtType(Type type) {
        if (isQtCanProvide(type)) {
            return static_cast<QFileIconProvider::IconType>(type);
        }
        return std::nullopt;
    }

    inline static QIcon getLogoIcon() { return QIcon(":/icons/logo.ico"); }

    inline static QFileIconProvider provider_{};

    inline static QIcon getIconFromQt(Type type) {
        return provider_.icon(toQtType(type).value_or(QFileIconProvider::File));
    }

    inline static QIcon getIconWithFileExtension(QString const& fileName) {
        return provider_.icon(QFileInfo(fileName));
    }

    inline static QIcon getIconOf(Type type) {
        char const* fakeFile = nullptr;

        switch (type) {
        case Type::Logo:
            return getLogoIcon();
        case Type::VideoFile:
            return getIconWithFileExtension(QStringLiteral("edm.mp4"));
        case Type::AudioFile:
            return getIconWithFileExtension(QStringLiteral("edm.mp3"));
        case Type::CompressedFile:
            return getIconWithFileExtension(QStringLiteral("edm.zip"));
        case Type::DocumentFile:
            return getIconWithFileExtension(QStringLiteral("edm.docx"));
        case Type::Application:
            return getIconWithFileExtension(QStringLiteral("edm.exe"));
        default:
            return getIconFromQt(type); // 剩余类型从 Qt 获取，如果获取不了 fallback 为文件
        }
    }
};

using IconType = EdmIconProvider::Type;

[[nodiscard]] inline QIcon getIcon(IconType type) { return EdmIconProvider::getIconOf(type); }


} // namespace edm::icon_utils