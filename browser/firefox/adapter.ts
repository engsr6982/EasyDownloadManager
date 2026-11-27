import { MENU_ID_IMAGE, MENU_ID_LINK } from "@common/api/types";
import type {
  CookieLike,
  DownloadLike,
  MenuInfo,
  PlatformApi,
  PortLike,
  RequestObservation,
  ResourceType,
  TabLike,
} from "@common/api/types";
import type { Header } from "@common/types/http";

const HTTP_URLS = ["http://*/*", "https://*/*"];

/** Firefox browser 命名空间使用 chrome 类型声明。 */
declare const browser: typeof chrome;

/** 五个观察事件形态一致，仅 details 子类型与 extraInfoSpec 不同，统一按只读观察形态引用 */
interface ObserveEvent {
  addListener(
    fn: (d: RawDetails) => void,
    filter: chrome.webRequest.RequestFilter,
    extra?: string[]
  ): void;
}
type ObservedName = "onBeforeRequest" | "onBeforeSendHeaders" | "onHeadersReceived" | "onCompleted" | "onErrorOccurred";
const observed = browser.webRequest as unknown as Record<ObservedName, ObserveEvent>;

/** webRequest 回调详情的按需投影，只声明用到的字段 */
interface RawDetails {
  requestId: string;
  method: string;
  url: string;
  type?: string;
  tabId?: number;
  frameId?: number;
  statusCode?: number;
  requestHeaders?: { name: string; value?: string }[];
  responseHeaders?: { name: string; value?: string }[];
}

/** 将标签页对象投影为通用字段。 */
function toTab(tab?: Partial<chrome.tabs.Tab> | TabLike | null): TabLike {
  return {
    id: tab?.id,
    url: tab?.url,
    pendingUrl: tab?.pendingUrl,
    title: tab?.title,
    openerTabId: tab?.openerTabId,
    active: tab?.active,
  };
}

/** 将宿主请求头投影为通用字段，缺省值填空串。 */
function toHeaders(list?: { name: string; value?: string }[]): Header[] | undefined {
  return list?.map((h) => ({ name: h.name, value: h.value ?? "" }));
}

/** 将 webRequest 详情投影为通用观察态。 */
function toObservation(details: RawDetails): RequestObservation {
  return {
    requestId: details.requestId,
    method: details.method,
    url: details.url,
    type: details.type ?? "other",
    tabId: details.tabId ?? -1,
    frameId: details.frameId ?? -1,
    status: details.statusCode,
    requestHeaders: toHeaders(details.requestHeaders),
    responseHeaders: toHeaders(details.responseHeaders),
  };
}

/** 注册只读 HTTP(S) 请求观察事件，并投影为通用字段。 */
function observe(name: ObservedName, handler: (d: RequestObservation) => void, types: ResourceType[], extra?: string[]): void {
  observed[name].addListener(
    (details) => handler(toObservation(details)),
    { urls: HTTP_URLS, types: types as chrome.webRequest.ResourceType[] },
    extra
  );
}

/** Firefox 平台实现，由 @platform 在构建期选定。 */
export const api: PlatformApi = {
  storageGet(keys) {
    return browser.storage.local.get(keys) as Promise<Record<string, unknown>>;
  },
  storageSet(items) {
    return browser.storage.local.set(items);
  },

  i18n(key) {
    return browser.i18n.getMessage(key) || key;
  },

  onMessage(handler) {
    browser.runtime.onMessage.addListener((message, sender, sendResponse) =>
      handler(
        message,
        { tab: sender.tab ? toTab(sender.tab) : undefined, frameId: sender.frameId },
        (r) => sendResponse(r)
      )
    );
  },
  sendMessageToTab(tabId, message, frameId) {
    const options = frameId === undefined ? {} : { frameId };
    void browser.tabs.sendMessage(tabId, message, options).catch(() => undefined);
  },
  sendMessage(message) {
    void browser.runtime.sendMessage(message).catch(() => undefined);
  },
  request(message) {
    return browser.runtime.sendMessage(message) as Promise<unknown>;
  },
  connect(name) {
    return browser.runtime.connect({ name }) as unknown as PortLike;
  },
  onConnect(handler) {
    browser.runtime.onConnect.addListener((port) => {
      const p = port as unknown as PortLike;
      handler({
        ...p,
        sender: p.sender ? { tab: toTab(p.sender.tab), frameId: p.sender.frameId, url: p.sender.url } : undefined,
      });
    });
  },

  onBeforeRequest(handler, types) {
    observe("onBeforeRequest", handler, types);
  },
  onBeforeSendHeaders(handler, types) {
    observe("onBeforeSendHeaders", handler, types, ["requestHeaders"]);
  },
  onHeadersReceived(handler, types) {
    observe("onHeadersReceived", handler, types, ["responseHeaders"]);
  },
  onCompleted(handler, types) {
    observe("onCompleted", handler, types);
  },
  onErrorOccurred(handler, types) {
    observe("onErrorOccurred", handler, types);
  },

  onHistoryStateUpdated(handler) {
    browser.webNavigation.onHistoryStateUpdated.addListener((d) =>
      handler({ tabId: d.tabId, frameId: d.frameId, url: d.url })
    );
  },

  getTab(tabId) {
    return browser.tabs
      .get(tabId)
      .then(toTab)
      .catch(() => undefined);
  },
  queryTabs() {
    return browser.tabs.query({}).then((tabs) => tabs.map(toTab));
  },
  removeTab(tabId) {
    void browser.tabs.remove(tabId).catch(() => undefined);
  },
  onTabRemoved(handler) {
    browser.tabs.onRemoved.addListener((tabId) => handler(tabId));
  },

  onDownloadCreated(handler) {
    browser.downloads.onCreated.addListener((item) =>
      handler({ id: item.id, url: item.url, finalUrl: item.finalUrl } as DownloadLike)
    );
  },
  cancelDownload(id) {
    void browser.downloads.cancel(id).catch(() => undefined);
  },
  /** 按原始或最终 URL 精确匹配最近 20 条下载。 */
  async findDownloadsByUrl(url) {
    const list = await browser.downloads.search({ limit: 20 });
    return list
      .filter((d) => d.url === url || d.finalUrl === url)
      .map((d) => ({ id: d.id, url: d.url, finalUrl: d.finalUrl, state: d.state }) as DownloadLike);
  },
  eraseDownload(id) {
    void browser.downloads.erase({ id }).catch(() => undefined);
  },
  removeDownloadFile(id) {
    void browser.downloads.removeFile(id).catch(() => undefined);
  },

  getCookies(url) {
    return browser.cookies.getAll({ url }) as Promise<CookieLike[]>;
  },

  createDownloadMenus(title) {
    // Firefox 的 contextMenus 无 documentUrlPatterns 选项，菜单范围由 contexts 与 common 侧判定限定
    browser.contextMenus.removeAll(() => {
      browser.contextMenus.create({ id: MENU_ID_LINK, title, contexts: ["link"] });
      browser.contextMenus.create({ id: MENU_ID_IMAGE, title, contexts: ["image"] });
    });
  },
  removeDownloadMenus() {
    browser.contextMenus.removeAll();
  },
  onMenuClicked(handler) {
    browser.contextMenus.onClicked.addListener((info, tab) => {
      const m: MenuInfo = {
        menuItemId: String(info.menuItemId),
        linkUrl: info.linkUrl,
        srcUrl: info.srcUrl,
        frameId: info.frameId ?? 0,
      };
      handler(m, toTab(tab));
    });
  },
};
