import { api } from "@platform";
import { PORT_RECONNECT_DELAY_MS } from "@common/data/constants";
import { createPanel } from "@common/panel/panel";
import { showToast } from "@common/panel/toast";
import type { PortLike } from "@common/api/types";
import type { BackgroundToContent, BackgroundToPort, PortToBackground } from "@common/types/messages";

const panel = createPanel((id) => {
  try {
    api.sendMessage({ kind: "panel:click", id });
  } catch {
    /* 扩展重载后旧页面里的上下文已失效，忽略 */
  }
});
let framePort: PortLike | null = null;

/** 后台 Service Worker 空闲终止会断开端口，按固定间隔重连以维持来源帧注册表 */
function connectFrame(): void {
  let port: PortLike;
  try {
    port = api.connect("edm-frame");
  } catch {
    return;
  }
  framePort = port;
  port.onMessage.addListener((msg) => {
    const m = msg as BackgroundToPort;
    if (m && m.type === "hello") reportFrame();
  });
  port.onDisconnect.addListener(() => {
    if (framePort === port) framePort = null;
    setTimeout(connectFrame, PORT_RECONNECT_DELAY_MS);
  });

  reportFrame();
  // document_start 时标题常未就绪，DOM 完成后再补报一次
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", () => reportFrame(), { once: true });
}

/** 向后台上报当前框架地址与标题。 */
function reportFrame(): void {
  const port = framePort;
  if (!port) return;
  const msg: PortToBackground = { type: "report", url: location.href, title: document.title };
  try {
    port.postMessage(msg);
  } catch {
    if (framePort === port) framePort = null;
  }
}

/** 处理后台推送的面板、重扫与一次性提示。 */
function onBackgroundMessage(message: unknown): void {
  const m = message as BackgroundToContent;
  if (!m || typeof m.kind !== "string") return;
  switch (m.kind) {
    case "panel:sync":
      panel.sync(m.items);
      return;
    case "panel:clear":
      panel.clear();
      return;
    case "rescan":
      reportFrame();
      panel.relayout();
      return;
    case "notify":
      showToast(m.message);
      return;
  }
}

/** 安装消息监听并连接来源帧端口。 */
export function startContent(): void {
  api.onMessage(onBackgroundMessage);
  connectFrame();
}
