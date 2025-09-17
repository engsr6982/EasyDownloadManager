#pragma once
#include <memory>
#include <qtclasshelpermacros.h>

class QThread;

namespace edm ::downloader {

class DownloadWorker final {
    std::unique_ptr<QThread> thread_;

public:
    Q_DISABLE_COPY_MOVE(DownloadWorker);
};

} // namespace edm::downloader
