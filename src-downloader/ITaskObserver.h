#pragma once
#include "Global.h"

#include <memory>
#include <string>

namespace edm {

struct TaskContext;

struct ITaskObserver {
    virtual ~ITaskObserver() = default;

    virtual void onStateChanged(std::shared_ptr<TaskContext>& ctx, TaskState oldState) = 0;

    virtual void onError(std::shared_ptr<TaskContext>& ctx, std::string const& error) = 0;

    virtual void onFileNameChanged(std::shared_ptr<TaskContext>& ctx) = 0;
};


} // namespace edm