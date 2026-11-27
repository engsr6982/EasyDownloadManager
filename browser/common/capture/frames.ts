import { api } from "@platform";
import type { BackgroundToPort, PortToBackground } from "@common/types/messages";

interface FrameInfo {
  url: string;
  title: string;
}

const key = (tabId: number, frameId: number) => `${tabId}:${frameId}`;
const frames = new Map<string, FrameInfo>();

/** 来源帧注册表：端口建立时按 tabId 与 frameId 记录文档地址与标题 */
export function installFrameRegistry(): void {
  api.onConnect((port) => {
    const tabId = port.sender?.tab?.id;
    const frameId = port.sender?.frameId ?? 0;
    if (tabId === undefined) return;
    const k = key(tabId, frameId);
    frames.set(k, {
      url: port.sender?.url ?? port.sender?.tab?.url ?? "",
      title: port.sender?.tab?.title ?? "",
    });

    const hello: BackgroundToPort = { type: "hello", frameId };
    port.postMessage(hello);
    port.onMessage.addListener((msg) => {
      const m = msg as PortToBackground;
      if (m && m.type === "report") frames.set(k, { url: m.url, title: m.title });
    });
  });

  // 单页应用换地址：更新主框架记录并请求内容脚本重扫
  api.onHistoryStateUpdated((d) => {
    if (d.frameId === 0) {
      const main = frames.get(key(d.tabId, 0));
      if (main) main.url = d.url;
    }
    api.sendMessageToTab(d.tabId, { kind: "rescan" });
  });
}

/** 返回指定来源帧的文档地址与标题。 */
export function getFrame(tabId: number, frameId: number): FrameInfo | undefined {
  return frames.get(key(tabId, frameId));
}

/** 移除标签页全部来源帧记录。 */
export function removeTabFrames(tabId: number): void {
  for (const k of [...frames.keys()]) {
    if (k.startsWith(`${tabId}:`)) frames.delete(k);
  }
}
