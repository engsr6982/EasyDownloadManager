#pragma once

namespace jspp {
struct ModuleMeta;
}

namespace edm::script::binding {

/// "edm" 原生模块（文件级静态元对象，生命周期与进程一致）。
jspp::ModuleMeta const& edmModule();

} // namespace edm::script::binding
