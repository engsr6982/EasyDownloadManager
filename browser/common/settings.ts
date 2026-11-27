import { api } from "@platform";
import { DEFAULT_HOST, DEFAULT_PORT, STORAGE_KEYS } from "@common/data/constants";

/** 扩展功能开关与引擎连接配置。 */
export interface Settings {
  /** 总开关，关闭后不捕获、不列面板、不建菜单 */
  enabled: boolean;
  /** 判定为下载的请求直接移交引擎 */
  autoCapture: boolean;
  /** 页面内浮动面板列出可下载的媒体与字幕 */
  sniffPanel: boolean;
  /** 链接与图片的右键菜单移交项 */
  contextMenu: boolean;
  host: string;
  port: number;
  token: string;
}

/** 后台冷启动与 popup 缺省值共用 */
export const DEFAULT_SETTINGS: Settings = {
  enabled: true,
  autoCapture: true,
  sniffPanel: true,
  contextMenu: true,
  host: DEFAULT_HOST,
  port: DEFAULT_PORT,
  token: "",
};

/** 由主机与端口生成引擎 WebSocket 地址。 */
export function wsUrl(s: Settings): string {
  return `ws://${s.host}:${s.port}`;
}

/** 开关缺省为开，仅显式写入 false 时关闭 */
export async function loadSettings(): Promise<Settings> {
  const stored = await api.storageGet(Object.values(STORAGE_KEYS));
  const on = (key: string): boolean => stored[key] !== false;
  const port = Number(stored[STORAGE_KEYS.port]);
  return {
    enabled: on(STORAGE_KEYS.enabled),
    autoCapture: on(STORAGE_KEYS.autoCapture),
    sniffPanel: on(STORAGE_KEYS.sniffPanel),
    contextMenu: on(STORAGE_KEYS.contextMenu),
    host: typeof stored[STORAGE_KEYS.host] === "string" && stored[STORAGE_KEYS.host] ? String(stored[STORAGE_KEYS.host]) : DEFAULT_HOST,
    port: Number.isFinite(port) && port > 0 ? port : DEFAULT_PORT,
    token: typeof stored[STORAGE_KEYS.token] === "string" ? String(stored[STORAGE_KEYS.token]) : "",
  };
}
