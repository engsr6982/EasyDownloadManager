/**
 * url-rewriter：下载前的 URL/文件名改写演示。
 *
 * 1. beforeFetchMeta（同步）：正则把 http 改写为 https；
 * 2. beforeFetchMeta（异步）：对 /redirect/ 链接先 HEAD 探测真实直链再改写；
 * 3. afterFetchMeta：文件名追加下载日期。
 */
import { events, logger, http, constants, Priority } from "edm";

export default class UrlRewriter {
    onEnable() {
        this.unlisteners = [
            // ---- 同步改写：http 改写为 https ----
            events.listen("beforeFetchMeta", (ev) => {
                const host = hostOf(ev.url);
                if (ev.url.startsWith("http://") && host && !isLocalHost(host)) {
                    const rewritten = ev.url.replace(/^http:\/\//, "https://");
                    logger.info("beforeFetchMeta: {} -> {}", ev.url, rewritten);
                    ev.url = rewritten;
                }
            }),

            // ---- 异步改写：HEAD 解析重定向后的真实直链 ----
            events.listen("beforeFetchMeta", (ev) => {
                if (!ev.url.includes("/redirect/")) return undefined; // 只处理演示链接
                return (async () => {
                    logger.info("解析重定向: {}", ev.url);
                    const res = await http.head(ev.url);
                    if (res.ok && res.finalUrl && res.finalUrl !== ev.url) {
                        logger.info("改写为直链: {}", res.finalUrl);
                        ev.url = res.finalUrl;
                    }
                })();
            }, Priority.High),

            // ---- 文件名追加日期 ----
            events.listen("afterFetchMeta", (ev) => {
                if (!ev.fileName || ev.fileName === constants.unknownFileName) return;
                const date = new Date().toISOString().slice(0, 10);
                ev.fileName = `${date}_${ev.fileName}`;
                logger.info("afterFetchMeta: fileSize={}, finalUrl={}, fileName={}", ev.fileSize, ev.finalUrl, ev.fileName);
            }),
        ];
    }

    onDisable() {
        for (const id of this.unlisteners ?? []) {
            events.unlisten(id);
        }
        this.unlisteners = [];
    }
}

function hostOf(url) {
    try {
        return new URL(url).hostname;
    } catch {
        return null;
    }
}

function isLocalHost(host) {
    return host === "localhost" || host === "127.0.0.1" || host === "[::1]";
}
