/**
 * proxy-demo：创建任务阶段为特定域名自动启用代理。
 * 演示 beforeCreateTask 钩子的同步字段改写（ev.useProxy = true）与 fs 持久化。
 */
import { events, fs, logger, path } from "edm";

const PROXY_HOSTS = ["example.com", "example.org"];
const HIT_COUNT_FILE = "hitcount.json"; // 相对路径，落在本脚本目录内

function loadHitCount() {
    const text = fs.readFile(HIT_COUNT_FILE);
    if (text === null) return 0;
    try {
        return Number(JSON.parse(text).hitCount) || 0;
    } catch {
        return 0;
    }
}

export default class ProxyDemo {
    onLoad() {
        logger.info("proxy-demo loaded");
    }

    onEnable() {
        this.hitCount = loadHitCount();
        this.unlisten = events.listen("beforeCreateTask", (ev) => {
            const host = safeHost(ev.url);
            if (host && PROXY_HOSTS.some((h) => host === h || host.endsWith("." + h))) {
                ev.useProxy = true;
                this.hitCount += 1;
                fs.writeFile(HIT_COUNT_FILE, JSON.stringify({ hitCount: this.hitCount }));
                logger.info("已为 {} 启用代理（累计 {} 次）", host, this.hitCount);
            }
        });
    }

    onDisable() {
        if (this.unlisten) {
            events.unlisten(this.unlisten);
            this.unlisten = null;
        }
        logger.debug("proxy-demo disabled");
    }
}

function safeHost(url) {
    try {
        return new URL(url).hostname;
    } catch {
        return null;
    }
}
