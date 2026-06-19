#pragma once
#include "downloader/FetchedMetaInfo.h"
#include "model/TaskModel.h"

#include <optional>


namespace edm {

struct TaskConfigure;
struct FetchedMetaInfo;

struct TaskContext {
    std::shared_ptr<TaskModel>       model;
    std::shared_ptr<TaskHeaderModel> header;
    std::shared_ptr<TaskConfigure>   configure;
    std::optional<FetchedMetaInfo>   meta;
};

} // namespace edm