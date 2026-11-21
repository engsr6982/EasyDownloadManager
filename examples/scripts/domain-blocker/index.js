/**
 * domain-blocker：创建任务阶段按域名黑名单拦截（beforeCreateTask 取消）。
 * 黑名单存脚本目录 blocklist.json（fs 持久化演示），规则支持精确域名与 "*.example.com" 通配。
 */
import { events, fs, logger, notify, NotifyLevel } from "edm";

const BLOCKLIST_FILE = "blocklist.json";

export default class DomainBlocker {
    onLoad() {
        // 黑名单在引擎就绪时读取；fs 相对路径以脚本目录为根
        this.blocklist = [];
        const raw = fs.readFile(BLOCKLIST_FILE);
        if (raw === null) {
            const defaults = ["ads.example.com", "*.tracker.example.net"];
            fs.writeFile(BLOCKLIST_FILE, JSON.stringify(defaults, null, 2));
            this.blocklist = defaults;
            logger.warn("{} 不存在，已写入默认黑名单", BLOCKLIST_FILE);
            return;
        }
        try {
            const parsed = JSON.parse(raw);
            if (Array.isArray(parsed)) {
                this.blocklist = parsed.filter((entry) => typeof entry === "string");
            }
        } catch (e) {
            logger.error("黑名单解析失败: {}", e);
        }
    }

    onEnable() {
        this.unlistener = events.listen("beforeCreateTask", (ev) => {
            const host = extractHost(ev.url);
            if (!host) return;

            if (this.matches(host)) {
                ev.cancel();
                logger.warn("已拦截 {}（命中黑名单规则 {}）", ev.url, host);
                notify(`已拦截来自 ${host} 的下载`, {
                    title: "domain-blocker",
                    level: NotifyLevel.Warning,
                });
            }
        });
        logger.info("domain-blocker enabled（黑名单 {} 条）", this.blocklist.length);
    }

    onDisable() {
        if (this.unlistener) {
            events.unlisten(this.unlistener);
            this.unlistener = null;
        }
    }

    matches(host) {
        for (const rule of this.blocklist) {
            const r = rule.toLowerCase();
            if (r.startsWith("*.")) {
                const base = r.slice(2);
                if (host === base || host.endsWith("." + base)) return true;
            } else if (host === r) {
                return true;
            }
        }
        return false;
    }
}

function extractHost(url) {
    // QuickJS 内建无 WHATWG URL，按协议头手工截取
    const m = /^https?:\/\/([^/:?#]+)/i.exec(url);
    return m ? m[1].toLowerCase() : null;
}
