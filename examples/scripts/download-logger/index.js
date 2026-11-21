/**
 * download-logger：订阅全部通知类钩子，把任务生命周期事件写入控制台，
 * 失败时弹托盘警告。每个任务只播报一次进度（10% 整数倍变化时）。
 */
import { events, logger, notify, utils, NotifyLevel } from "edm";

export default class DownloadLogger {
    onEnable() {
        this.lastPercent = new Map(); // taskId -> 上次播报的整数百分比

        this.unlisteners = [
            events.listen("taskStarted", (ev) => {
                logger.info("#{} 开始下载: {} ({})", ev.taskId, ev.fileName, ev.url);
                this.lastPercent.set(ev.taskId, -1);
            }),

            events.listen("taskResumed", (ev) => {
                logger.info("#{} 继续下载: {}", ev.taskId, ev.fileName);
            }),

            events.listen("taskPaused", (ev) => {
                logger.info("#{} 已暂停: {}", ev.taskId, ev.fileName);
            }),

            events.listen("taskProgress", (ev) => {
                if (ev.percent < 0) return; // 总大小未知
                const bucket = Math.floor(ev.percent / 10) * 10;
                const last = this.lastPercent.get(ev.taskId);
                if (bucket > last) {
                    this.lastPercent.set(ev.taskId, bucket);
                    logger.info(
                        "#{} 进度 {}%（{}/{}，速度 {}）",
                        ev.taskId,
                        Math.floor(ev.percent),
                        utils.formatBytes(ev.downloaded),
                        utils.formatBytes(ev.total),
                        utils.formatSpeed(ev.speed)
                    );
                }
            }),

            events.listen("taskCompleted", (ev) => {
                logger.info("#{} 下载完成: {}", ev.taskId, ev.fileName);
                this.lastPercent.delete(ev.taskId);
            }),

            events.listen("taskFailed", (ev) => {
                logger.error("#{} 下载失败: {} ({})", ev.taskId, ev.fileName, ev.error);
                this.lastPercent.delete(ev.taskId);
                notify(`下载失败: ${ev.fileName}（${ev.error}）`, { level: NotifyLevel.Warning });
            }),

            events.listen("taskCanceled", (ev) => {
                logger.warn("#{} 已取消: {}", ev.taskId, ev.fileName);
                this.lastPercent.delete(ev.taskId);
            }),

            events.listen("taskRemoved", (ev) => {
                logger.debug("#{} 已移除", ev.taskId);
                this.lastPercent.delete(ev.taskId);
            }),
        ];

        logger.debug("download-logger enabled, {} listeners", this.unlisteners.length);
    }

    onDisable() {
        for (const id of this.unlisteners ?? []) {
            events.unlisten(id);
        }
        this.unlisteners = [];
        this.lastPercent?.clear();
    }
}
