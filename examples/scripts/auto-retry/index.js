/**
 * auto-retry：任务失败后自动续传重试（指数退避），超过上限后托盘告警。
 * 演示通知类钩子 + tasks 控制 + timers 定时器的组合。
 */
import { events, logger, notify, tasks, NotifyLevel } from "edm";

const MAX_RETRIES = 3;      // 每任务最大自动重试次数
const BASE_DELAY_MS = 5000; // 首次重试延迟，之后逐次翻倍

export default class AutoRetry {
    onEnable() {
        this.retries = new Map(); // taskId -> 已发起的重试次数
        this.pending = new Set(); // 已安排退避重试的任务（避免失败风暴期重复调度）

        this.unlisteners = [
            events.listen("taskFailed", (ev) => {
                const count = this.retries.get(ev.taskId) ?? 0;
                if (count >= MAX_RETRIES) {
                    logger.error("#{} 连续 {} 次重试仍失败，放弃: {}", ev.taskId, MAX_RETRIES, ev.error);
                    notify(`自动重试 ${MAX_RETRIES} 次仍失败`, {
                        title: ev.fileName,
                        level: NotifyLevel.Error,
                    });
                    return;
                }
                if (this.pending.has(ev.taskId)) return;

                const delay = BASE_DELAY_MS * (2 ** count);
                const attempt = count + 1;
                this.pending.add(ev.taskId);
                logger.warn("#{} 失败（{}），{}ms 后自动重试 {}/{}", ev.taskId, ev.error, delay, attempt, MAX_RETRIES);

                setTimeout(() => {
                    this.pending.delete(ev.taskId);
                    if (!tasks.get(ev.taskId)) return; // 任务已被删除
                    if (tasks.resume(ev.taskId)) {
                        this.retries.set(ev.taskId, attempt);
                        logger.info("#{} 已发起第 {} 次重试", ev.taskId, attempt);
                    } else {
                        logger.error("#{} 重试请求未生效", ev.taskId);
                    }
                }, delay);
            }),

            events.listen("taskCompleted", (ev) => {
                if (this.retries.has(ev.taskId)) {
                    logger.info("#{} 重试 {} 次后完成", ev.taskId, this.retries.get(ev.taskId));
                    this.retries.delete(ev.taskId);
                }
            }),

            events.listen("taskRemoved", (ev) => {
                this.retries.delete(ev.taskId);
                this.pending.delete(ev.taskId);
            }),
        ];

        logger.info("auto-retry enabled, {} listeners", this.unlisteners.length);
    }

    onDisable() {
        for (const id of this.unlisteners ?? []) {
            events.unlisten(id);
        }
        this.unlisteners = [];
    }
}
