// 浏览器 API 差异适配层接口。common 全部经此访问宿主 API，编译期由 @platform 选定实现，
// 不做运行时能力探测。chromium 走 chrome 命名空间，firefox 走 browser 命名空间。

import type { Header } from "@common/types/http";

/** 下载域 Cookie 的精简字段。 */
export interface CookieLike {
  name: string;
  value: string;
}

/** 标签页上下文，可选字段对应宿主 API 的可缺省值。 */
export interface TabLike {
  id?: number;
  url?: string;
  pendingUrl?: string;
  title?: string;
  openerTabId?: number;
  active?: boolean;
}

/** 浏览器下载条目的精简字段。 */
export interface DownloadLike {
  id: number;
  url: string;
  finalUrl?: string;
  /** 下载状态，仅查询结果需要。 */
  state?: string;
}

/** webRequest 观察回调拿到的精简字段 */
export interface RequestObservation {
  requestId: string;
  method: string;
  url: string;
  type: string;
  tabId: number;
  frameId: number;
  status?: number;
  responseHeaders?: Header[];
  requestHeaders?: Header[];
}

/** 内容脚本与后台长连接端口的精简契约。 */
export interface PortLike {
  name: string;
  sender?: { tab?: TabLike; frameId?: number; url?: string };
  onMessage: { addListener(fn: (msg: unknown) => void): void };
  onDisconnect: { addListener(fn: () => void): void };
  postMessage(msg: unknown): void;
}

/** 右键菜单回调的精简字段 */
export interface MenuInfo {
  menuItemId: string;
  linkUrl?: string;
  srcUrl?: string;
  frameId: number;
}

/** 参与请求观察的 webRequest 资源类型。 */
export type ResourceType =
  | "main_frame"
  | "sub_frame"
  | "object"
  | "xmlhttprequest"
  | "media"
  | "image"
  | "other";

/** 链接右键菜单项标识，common 与适配层共用。 */
export const MENU_ID_LINK = "edm-link";
/** 图片右键菜单项标识，common 与适配层共用。 */
export const MENU_ID_IMAGE = "edm-image";

/** common 使用的宿主 API 契约，Promise 与回调差异由适配层处理。 */
export interface PlatformApi {
  // storage.local
  storageGet(keys: string[]): Promise<Record<string, unknown>>;
  storageSet(items: Record<string, unknown>): Promise<void>;

  // 词条（_locales，构建期由 assets/i18n 生成；缺词条原样返回 key）
  i18n(key: string): string;

  // runtime
  onMessage(
    handler: (
      message: unknown,
      sender: { tab?: TabLike; frameId?: number },
      sendResponse: (r?: unknown) => void
    ) => boolean | void
  ): void;
  sendMessageToTab(tabId: number, message: unknown, frameId?: number): void;
  sendMessage(message: unknown): void;
  request(message: unknown): Promise<unknown>;
  connect(name: string): PortLike;
  onConnect(handler: (port: PortLike) => void): void;

  // webRequest 观察（不拦截）
  onBeforeRequest(handler: (d: RequestObservation) => void, types: ResourceType[]): void;
  onBeforeSendHeaders(handler: (d: RequestObservation) => void, types: ResourceType[]): void;
  onHeadersReceived(handler: (d: RequestObservation) => void, types: ResourceType[]): void;
  onCompleted(handler: (d: RequestObservation) => void, types: ResourceType[]): void;
  onErrorOccurred(handler: (d: RequestObservation) => void, types: ResourceType[]): void;

  // webNavigation
  onHistoryStateUpdated(handler: (d: { tabId: number; frameId: number; url: string }) => void): void;

  // tabs
  getTab(tabId: number): Promise<TabLike | undefined>;
  queryTabs(): Promise<TabLike[]>;
  removeTab(tabId: number): void;
  onTabRemoved(handler: (tabId: number) => void): void;

  // downloads
  onDownloadCreated(handler: (d: DownloadLike) => void): void;
  /** 按 URL 反查近期下载条目：移交与下载创建事件先后顺序不定 */
  findDownloadsByUrl(url: string): Promise<DownloadLike[]>;
  cancelDownload(id: number): void;
  eraseDownload(id: number): void;
  removeDownloadFile(id: number): void;

  // cookies
  getCookies(url: string): Promise<CookieLike[]>;

  // contextMenus（菜单项在 Service Worker 每次唤醒时重建，内部先 removeAll 去重）
  createDownloadMenus(title: string): void;
  removeDownloadMenus(): void;
  onMenuClicked(handler: (info: MenuInfo, tab: TabLike) => void): void;
}
