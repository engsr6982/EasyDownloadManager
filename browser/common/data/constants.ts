// 跨模块共享的常量与阈值。调整行为参数集中改这里。

/** 默认引擎主机。 */
export const DEFAULT_HOST = "127.0.0.1";
/** 默认引擎端口。 */
export const DEFAULT_PORT = 26111;

/** 单条任务序列化后的字节上限，超长时拒绝移交并提示。 */
export const MAX_PAYLOAD_BYTES = 128 * 1024;

/** handedUrls 登记条目存活时间：移交后等待浏览器下载事件出现的窗口 */
export const HANDED_URL_TTL_MS = 60_000;

/** 嗅探面板媒体条目的最小体积（小于该值视为提示音/广告碎片，不提示） */
export const MIN_SNIFF_BYTES = 200 * 1024;

/** 小 ASF 视为广告素材的上界 */
export const MAX_ASF_BYTES = 1024 * 1024;

/** 后台推送面板条目的延迟，给页面媒体元素留出渲染时间以便锚定 */
export const PANEL_PUSH_DELAY_MS = 2000;

/** 面板条目轮询重定位间隔（锚元素可能因布局变化移动） */
export const PANEL_RELAYOUT_INTERVAL_MS = 1200;

/** 面板无操作后转半透明的等待。 */
export const PANEL_FADE_IDLE_MS = 30_000;
/** 面板移出后重新变暗的等待。 */
export const PANEL_FADE_OUT_MS = 15_000;

/** 面板条目在后台缓存的最长存活时间，超时未点击即回收 */
export const PANEL_ITEM_TTL_MS = 5 * 60_000;

/** 建链后等待就绪再发送队列的窗口，超时即放弃排队。 */
export const READY_WAIT_MS = 1500;

/** 弹窗连接测试的等待上界 */
export const CONNECTION_TEST_TIMEOUT_MS = 3000;

/** 内容脚本长连接断开后的重连等待（后台 Service Worker 空闲终止会断开端口） */
export const PORT_RECONNECT_DELAY_MS = 1000;

/** popup 与后台共用的 storage 键名 */
export const STORAGE_KEYS = {
  enabled: "enabled",
  autoCapture: "autoCapture",
  sniffPanel: "sniffPanel",
  contextMenu: "contextMenu",
  host: "host",
  port: "port",
  token: "token",
} as const;
