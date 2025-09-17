#pragma once
#include <qtclasshelpermacros.h>


namespace edm ::downloader {

struct TaskConfigure;

class MultiThreadDownloader {
    TaskConfigure const& configure_;

public:
    Q_DISABLE_COPY_MOVE(MultiThreadDownloader);

    explicit MultiThreadDownloader(TaskConfigure const& configure);
};

} // namespace edm::downloader
