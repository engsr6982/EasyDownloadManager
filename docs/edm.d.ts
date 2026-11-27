/**
 * EasyDownloadManager 脚本 API 类型声明。
 *
 * TypeScript 项目引用方式：
 *   import { events, logger } from "edm";
 *   // tsconfig.json 的 "include"/"files" 中加入本文件
 *
 * 入口约定：入口模块 `export default class`，类实例由宿主创建并按
 * onLoad、onEnable、onDisable 顺序驱动（三个方法均可选）。
 *
 * @example
 * import { events, logger } from "edm";
 *
 * export default class {
 *     onEnable() {
 *         events.listen("beforeCreateTask", (ev) => {
 *             ev.useProxy = true;
 *             logger.info("已启用代理");
 *         });
 *     }
 * }
 */

declare module "edm" {
    /** 脚本入口类约定（三个生命周期方法均为可选） */
    export interface PluginEntry {
        /** 模块加载完成（引擎已就绪）；钩子注册位于 onEnable */
        onLoad?(): void;
        /** 启用：注册监听器/定时器等 */
        onEnable?(): void;
        /** 禁用：与 onEnable 对称地反注册（监听器/定时器随引擎卸载自动失效） */
        onDisable?(): void;
    }

    /** 监听优先级：数值小的先派发，同优先级按注册顺序 */
    export enum Priority {
        /** 最高 */
        Highest = 0,
        High = 1,
        /** 默认 */
        Normal = 2,
        Low = 3,
        Lowest = 4,
    }

    /** 任务状态 */
    export enum TaskState {
        Pending = 0,
        Running = 1,
        Paused = 2,
        Canceled = 3,
        Finished = 4,
        Failed = 5,
        Preparing = 6,
    }

    /** 托盘通知级别 */
    export enum NotifyLevel {
        Info = 0,
        Warning = 1,
        Error = 2,
    }

    /** 引擎内部语义常量（脚本侧不应硬编码这些值） */
    export const constants: {
        /** 无效任务 id */
        readonly invalidTaskId: number;
        /** 未解析文件名的占位值 */
        readonly unknownFileName: string;
        /** 未知文件大小 */
        readonly unknownFileSize: number;
        /** 未知进度百分比 */
        readonly unknownPercent: number;
    };

    /** 监听器句柄 */
    export type ListenerId = number;

    /** 可取消事件公共成员（由 Cancellable 事件自动携带） */
    export interface CancellableEvent {
        /** 是否已被取消 */
        isCancelled(): boolean;
        /** 取消原因（cancel(reason) 时传入的内容） */
        cancelReason(): string;
        /** 取消当前操作并停止向后续监听器派发，可选携带原因 */
        cancel(reason?: string): void;
        /** 直接置位/复位取消标记 */
        setCancelled(v: boolean): void;
    }

    /** beforeCreateTask 事件：TaskModel 组装后、落库前（仅同步 handler） */
    export interface BeforeCreateTaskEvent extends CancellableEvent {
        /** 任务 id（创建阶段尚未分配，恒为 invalidTaskId） */
        readonly taskId: number;
        /** 下载地址（可改写；异步改写使用 beforeFetchMeta） */
        url: string;
        /** 占位文件名（可改写） */
        fileName: string;
        /** 保存目录（可改写） */
        saveDir: string;
        /** 带宽限制 KB/s，0 = 不限（可改写；JS 侧为 BigInt） */
        bandLimit: bigint;
        /** 线程数 4/8/16/32（可改写） */
        threadCount: number;
        /** User-Agent（可改写） */
        userAgent: string;
        /** Origin（可改写；null/"" 表示清除） */
        origin: string | null;
        /** Referer（可改写） */
        referer: string | null;
        /** Cookie（可改写） */
        cookie: string | null;
        /** 自定义请求头数组（可改写） */
        headers: string[];
        /** 是否使用全局代理（可改写） */
        useProxy: boolean;
        /** 代理地址，会话级不持久化（可改写） */
        proxyUrl: string;
        /** 是否覆盖同名文件（可改写） */
        overwriteExisting: boolean;
    }

    /** beforeFetchMeta 事件：元信息探测前（支持异步 handler） */
    export interface BeforeFetchMetaEvent extends CancellableEvent {
        readonly taskId: number;
        /** 下载地址（改写点一：页面链接到直链；改写结果持久化） */
        url: string;
        userAgent: string;
        origin: string | null;
        referer: string | null;
        cookie: string | null;
        headers: string[];
        useProxy: boolean;
        proxyUrl: string;
    }

    /** afterFetchMeta 事件：元信息与文件名确定后、磁盘预分配前（支持异步 handler） */
    export interface AfterFetchMetaEvent extends CancellableEvent {
        readonly taskId: number;
        url: string;
        userAgent: string;
        origin: string | null;
        referer: string | null;
        cookie: string | null;
        headers: string[];
        /** 文件名（改写点二：改写后引擎据此写盘，UI/数据库同步更新） */
        fileName: string;
        /** 重定向后的最终地址 */
        readonly finalUrl: string;
        /** 是否支持断点续传 */
        readonly supportRange: boolean;
        /** 文件大小（未知为 unknownFileSize；JS 侧为 BigInt） */
        readonly fileSize: bigint;
        readonly contentType: string | null;
        readonly etag: string | null;
        readonly lastModified: string | null;
        readonly contentDisposition: string | null;
        readonly md5: string | null;
    }

    /** 通知类事件（taskStarted/taskProgress/...，只读） */
    export interface TaskEvent {
        readonly taskId: number;
        readonly url: string;
        readonly fileName: string;
        /** 仅 taskFailed 携带错误信息 */
        readonly error: string;
        /** 已下载字节 */
        readonly downloaded: number;
        /** 总字节（未知为 unknownFileSize） */
        readonly total: number;
        /** 速度，字节/秒 */
        readonly speed: number;
        /** 0-100，未知为 unknownPercent */
        readonly percent: number;
    }

    /** 事件监听器签名；可变钩子的 handler 可返回 Promise 做异步改写 */
    export type Listener<T> = (ev: T) => void | Promise<void>;

    /**
     * 生命周期事件总线。
     *
     * 监听器按 Priority、脚本加载序、注册序派发；单个监听器抛出的异常
     * 写入该脚本的控制台，不影响其他监听器；cancel() 后停止向后续监听器派发。
     * 恢复下载会再次触发准备阶段钩子，改写逻辑需保持幂等。
     */
    export const events: {
        /**
         * 注册事件监听器
         * @param event    钩子名："beforeCreateTask" | "beforeFetchMeta" | "afterFetchMeta"
         *                 | "taskStarted" | "taskProgress" | "taskPaused" | "taskResumed"
         *                 | "taskCompleted" | "taskFailed" | "taskCanceled" | "taskRemoved"
         * @param handler  事件回调；beforeFetchMeta/afterFetchMeta 可返回 Promise
         * @param priority 监听优先级，默认 Normal
         * @returns 监听器 id（供 unlisten）
         * @throws 传入未知事件名时抛出 TypeError
         */
        listen(event: "beforeCreateTask", handler: Listener<BeforeCreateTaskEvent>, priority?: Priority): ListenerId;
        listen(event: "beforeFetchMeta", handler: Listener<BeforeFetchMetaEvent>, priority?: Priority): ListenerId;
        listen(event: "afterFetchMeta", handler: Listener<AfterFetchMetaEvent>, priority?: Priority): ListenerId;
        listen(
            event: "taskStarted" | "taskProgress" | "taskPaused" | "taskResumed" | "taskCompleted" | "taskFailed"
                | "taskCanceled" | "taskRemoved",
            handler: Listener<TaskEvent>,
            priority?: Priority
        ): ListenerId;
        /** 移除监听器 @returns 是否移除成功 */
        unlisten(id: ListenerId): boolean;
    };

    /**
     * 命令面板。注册的命令显示在主窗口命令面板（Ctrl+Shift+P 呼出），
     * handler 在主线程执行；引擎卸载时全部自动注销。
     */
    export const commands: {
        /** @returns 句柄，供 unregister */
        register(title: string, description: string, handler: () => void): number;
        /** @returns 是否移除成功 */
        unregister(id: number): boolean;
    };

    /** 日志输出（写入脚本自身的虚拟控制台；"{}" 占位符依次替换后续参数） */
    export const logger: {
        info(fmt: string, ...args: unknown[]): void;
        warn(fmt: string, ...args: unknown[]): void;
        error(fmt: string, ...args: unknown[]): void;
        debug(fmt: string, ...args: unknown[]): void;
    };

    /**
     * 文件访问（文本读写）。
     * 路径契约：POSIX 风格为主，兼容 Windows 风格输入；
     * 相对路径以脚本所在目录为根，绝对路径原样使用。
     * 布尔返回值表示操作是否成功；OS 级拒绝（权限等）抛出异常。
     */
    export const fs: {
        exists(path: string): boolean;
        /** 读取文本文件 @returns 非普通文件（不存在、目录等）返回 null */
        readFile(path: string): string | null;
        /** 写入文本文件（自动创建缺失父目录）@returns 数据是否完整落盘 */
        writeFile(path: string, content: string): boolean;
        /** 拷贝文件或目录树，覆盖同名 */
        copy(src: string, dst: string): boolean;
        /** 同卷 rename，跨卷回退为拷贝后删除 */
        move(src: string, dst: string): boolean;
        /** 仅创建单级目录，父目录必须存在 @returns 目录已存在时返回 false */
        mkdir(path: string): boolean;
        /** 创建目录并补齐缺失父目录 @returns 目录已存在时返回 false */
        mkdir(path: string, recursive: boolean): boolean;
        /** 追加写入 @returns false 当 createIfNotExists=false 且文件不存在 */
        appendFile(path: string, content: string, createIfNotExists?: boolean): boolean;
        /** 递归删除 @returns 是否有条目被删除 */
        remove(path: string): boolean;
        isDirectory(path: string): boolean;
        isFile(path: string): boolean;
    };

    /** POSIX 路径工具（对应 Node.js path.posix；'' 视作分隔符并归一化为 '/'） */
    export const path: {
        /** 拼接非空段并归一化（全空返回 "."） */
        join(...parts: string[]): string;
        basename(path: string): string;
        dirname(path: string): string;
        extname(path: string): string;
        /** from 到 to 的相对路径 */
        relative(from: string, to: string): string;
        /** 以脚本目录为根解析（绝对路径归一化后原样返回） */
        absolute(path: string): string;
        normalize(path: string): string;
    };

    /** HTTP 响应 */
    export interface HttpResponse {
        /** HTTP 状态码 */
        readonly status: number;
        /** 是否 2xx */
        readonly ok: boolean;
        /** 重定向后的最终地址 */
        readonly finalUrl: string;
        /** 响应体（head 为空字符串；上限 16MB） */
        readonly body: string;
        /** 响应头（同名取最后一个） */
        readonly headers: Record<string, string>;
        /** 传输层错误（ok=false 时有效） */
        readonly error: string;
    }

    export interface HttpOptions {
        headers?: Record<string, string>;
        proxy?: string;
        timeoutMs?: number;
    }

    /** HTTP 客户端（异步；供改写类脚本解析直链/调用接口） */
    export const http: {
        get(url: string, options?: HttpOptions): Promise<HttpResponse>;
        head(url: string, options?: HttpOptions): Promise<HttpResponse>;
    };

    /** 任务信息快照 */
    export interface TaskInfo {
        readonly id: number;
        readonly url: string;
        readonly fileName: string;
        readonly saveDir: string;
        readonly state: TaskState;
        /** 文件大小（JS 侧为 BigInt） */
        readonly fileSize: bigint;
        readonly downloaded: number;
        readonly speed: number;
        readonly threadCount: number;
    }

    /** 任务查询与控制（操作经宿主队列异步执行） */
    export const tasks: {
        get(id: number): TaskInfo | null;
        getAll(): TaskInfo[];
        pause(id: number): boolean;
        resume(id: number): boolean;
        cancel(id: number): boolean;
    };

    /** 定时器（回调在主线程执行） */
    export function setTimeout(callback: () => void, ms?: number): number;
    export function clearTimeout(id: number): boolean;
    export function setInterval(callback: () => void, ms?: number): number;
    export function clearInterval(id: number): boolean;

    /** 通用格式化（与主程序显示格式一致） */
    export const utils: {
        /** "12.34 MB"（两位小数，自动单位） */
        formatBytes(bytes: number): string;
        /** "1.23 MB/s" */
        formatSpeed(bytesPerSecond: number): string;
    };

    /**
     * 托盘通知（经宿主系统托盘弹出）
     * @param message 消息内容
     * @param options.title 默认为脚本名；options.level 默认 Info
     * @throws 托盘不可用或级别非法时抛出异常
     */
    export function notify(message: string, options?: { title?: string; level?: NotifyLevel }): void;
}
