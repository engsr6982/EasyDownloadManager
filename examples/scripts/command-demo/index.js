/**
 * command-demo：向命令面板注册脚本命令（Ctrl+Shift+P 呼出）。
 * 演示 commands.register/unregister 与引擎卸载时的自动清理。
 */
import { commands, logger, notify } from "edm";

export default class CommandDemo {
    onEnable() {
        this.handle = commands.register(
            "示例：记录一条日志",
            "向本脚本的控制台写入一条测试日志",
            () => {
                logger.info("命令面板触发于 {}", new Date().toLocaleTimeString());
            }
        );

        this.handle2 = commands.register(
            "示例：发送托盘通知",
            "弹出一条 Info 级托盘通知",
            () => {
                notify("命令面板触发成功", { title: "command-demo" });
            }
        );

        logger.info("command-demo enabled, {} commands", 2);
    }

    onDisable() {
        // 显式注销演示；引擎卸载时未注销的命令也会被自动清理
        if (this.handle) commands.unregister(this.handle);
        if (this.handle2) commands.unregister(this.handle2);
        this.handle = null;
        this.handle2 = null;
    }
}
