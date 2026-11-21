/**
 * dedupe-guard：阻止为同一 URL 重复创建进行中的任务。
 * 演示 beforeCreateTask 的取消语义 + tasks.getAll 快照查询 + constants 魔术值。
 */
import { constants, events, logger, notify, tasks, NotifyLevel, TaskState } from "edm";

// 视为"占用中"的状态：进行中/等待/暂停的旧任务才拦截，历史完成的直接放行
const ACTIVE_STATES = new Set([
    TaskState.Pending,
    TaskState.Running,
    TaskState.Preparing,
    TaskState.Paused,
]);

export default class DedupeGuard {
    onEnable() {
        this.unlistener = events.listen("beforeCreateTask", (ev) => {
            // 新任务尚未分配 id（taskId 恒为 invalidTaskId），只能按 URL 对比存量任务
            for (const task of tasks.getAll()) {
                if (task.id === constants.invalidTaskId) continue;
                if (task.url !== ev.url) continue;
                if (!ACTIVE_STATES.has(task.state)) continue;

                ev.cancel();
                logger.warn("已拦截重复任务: {}（与 #{} 冲突）", ev.url, task.id);
                notify("已拦截重复下载任务", {
                    title: task.fileName,
                    level: NotifyLevel.Warning,
                });
                return;
            }
        });
        logger.info("dedupe-guard enabled");
    }

    onDisable() {
        if (this.unlistener) {
            events.unlisten(this.unlistener);
            this.unlistener = null;
        }
    }
}
