# EasyDownloadManager

基于 Qt 6 的 Windows 多线程下载器，Fluent Design 风格界面。内建脚本扩展系统，
可以在下载的各个阶段改写行为、监听任务生命周期。

## 功能

下载引擎支持多线程、断点续传、带宽限制与代理，任务状态持久化到数据库。
任务卡片实时显示进度与速度，配合任务详情抽屉查看速率、剩余时间与线程信息。

脚本扩展（[开发文档](docs/scripting.md)）基于生命周期钩子：脚本可在任务创建、
元信息探测、文件命名阶段改写 URL、请求头、代理与文件名，也可订阅开始、进度、
完成等通知事件。命令面板（Ctrl+Shift+P）聚合应用操作与脚本命令。
插件中心提供启用、禁用、重载与虚拟控制台实时日志。

## 构建

Windows 环境，Qt 6.8（MSVC 2022）、CMake、Ninja，依赖经 vcpkg 与 FetchContent 解析
（quickjs-ng 与 jspp 首次配置时从 GitHub 拉取）：

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## 文档

- [脚本扩展开发](docs/scripting.md)
- [脚本 API 类型声明](docs/edm.d.ts)
- [Agent 与维护者指引](AGENTS.md)
