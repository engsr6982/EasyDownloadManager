import { api } from "@platform";
import type { DownloadLike } from "@common/api/types";
import { HANDED_URL_TTL_MS, PANEL_PUSH_DELAY_MS, PANEL_ITEM_TTL_MS } from "@common/data/constants";
import { removeTabFrames } from "@common/capture/frames";
import { DEFAULT_SETTINGS, loadSettings, type Settings } from "@common/settings";
import { buildJobFromPending } from "@common/capture/job";
import type { Pending } from "@common/types/pipeline";
import type { ConnectionTestResult, PanelItemView } from "@common/types/messages";
import { WsClient } from "@common/ws/client";

/** 后台共用的引擎 WebSocket 客户端。 */
const ws = new WsClient();

let settings: Settings = DEFAULT_SETTINGS;
/** 最近一次移交所属标签页，用于引擎离线提示。 */
let noticeTab: number | undefined;

interface Candidate {
  pending: Pending;
  view: PanelItemView;
  expires: number;
}
/** 全局候选表，键为条目 id，面板点击按 id 解析 */
const candidates = new Map<string, Candidate>();
/** 面板条目 id 列表，键为 tabId 与 frameId 的组合 */
const frameItems = new Map<string, string[]>();
/** 已移交 URL 登记：key 为最终 URL，value 为移交时间戳。 */
const handedUrls = new Map<string, number>();
const syncTimers = new Map<string, ReturnType<typeof setTimeout>>();

/** 重读设置并应用到 WebSocket 客户端。 */
export async function refreshSettings(): Promise<Settings> {
  settings = await loadSettings();
  ws.configure(settings);
  return settings;
}

/** 自动捕获移交，受总开关与自动移交开关约束。 */
export async function handOff(p: Pending): Promise<void> {
  if (!settings.enabled || !settings.autoCapture) return;
  await submit(p);
}

/** 面板点击与右键菜单的移交，只受总开关约束 */
export async function handOffExplicit(p: Pending): Promise<void> {
  if (!settings.enabled) return;
  await submit(p);
}

/** 组装并发送任务，发送成功后登记浏览器下载取消。 */
async function submit(p: Pending): Promise<void> {
  const job = await buildJobFromPending(p);
  if (!job) return;
  noticeTab = p.tabId;
  const ok = await ws.submit(job);
  if (ok) registerHanded(p.url);
}

let panelSeq = 0;
const frameKey = (tabId: number, frameId: number) => `${tabId}:${frameId}`;

/** 保存面板候选，并按来源帧合并延迟推送。 */
export function pushPanelCandidate(
  tabId: number,
  pending: Pending,
  label: string,
  sub: string
): void {
  if (!settings.enabled || !settings.sniffPanel) return;
  pruneCandidates();
  const id = `p${++panelSeq}`;
  const view: PanelItemView = { id, label, sub, url: pending.url };
  candidates.set(id, { pending, view, expires: Date.now() + PANEL_ITEM_TTL_MS });
  const key = frameKey(tabId, pending.frameId);
  const list = frameItems.get(key) ?? [];
  list.push(id);
  frameItems.set(key, list);
  scheduleSync(key, tabId, pending.frameId);
}

function scheduleSync(key: string, tabId: number, frameId: number): void {
  const existing = syncTimers.get(key);
  if (existing) clearTimeout(existing);
  // 延迟推送给页面媒体元素留渲染时间以便锚定；合并同帧多次推送
  syncTimers.set(
    key,
    setTimeout(() => {
      syncTimers.delete(key);
      flushFrame(key, tabId, frameId);
    }, PANEL_PUSH_DELAY_MS)
  );
}

function flushFrame(key: string, tabId: number, frameId: number): void {
  const ids = (frameItems.get(key) ?? []).filter((id) => candidates.has(id));
  frameItems.set(key, ids);
  const items: PanelItemView[] = ids.map((id) => candidates.get(id)!.view);
  api.sendMessageToTab(tabId, { kind: "panel:sync", items }, frameId);
}

/** 按条目 id 重新组装任务，发送成功后移除候选。 */
export async function onPanelClick(tabId: number, id: string): Promise<void> {
  if (!settings.enabled) return;
  const cand = candidates.get(id);
  if (!cand) return;
  const job = await buildJobFromPending(cand.pending);
  if (!job) return;
  noticeTab = tabId;
  const ok = await ws.submit(job);
  if (ok) {
    registerHanded(cand.pending.url);
    candidates.delete(id);
    const key = frameKey(tabId, cand.pending.frameId);
    frameItems.set(
      key,
      (frameItems.get(key) ?? []).filter((x) => x !== id)
    );
    flushFrame(key, tabId, cand.pending.frameId);
  }
}

/** 清空后台候选与各标签页面板。 */
export async function clearPanels(): Promise<void> {
  candidates.clear();
  frameItems.clear();
  for (const [key, timer] of syncTimers) {
    clearTimeout(timer);
    syncTimers.delete(key);
  }
  const tabs = await api.queryTabs();
  for (const tab of tabs) if (tab.id !== undefined) api.sendMessageToTab(tab.id, { kind: "panel:clear" });
}


/** 登记已移交 URL，并反查已出现的浏览器下载。 */
function registerHanded(url: string): void {
  handedUrls.set(url, Date.now());
  pruneHanded();
  // 浏览器下载可在移交送达前开始，创建事件已错过：按 URL 反查同一条目再撤除
  void dropMatching(url);
}

function pruneHanded(): void {
  const now = Date.now();
  for (const [url, ts] of handedUrls) if (now - ts > HANDED_URL_TTL_MS) handedUrls.delete(url);
}

async function dropMatching(url: string): Promise<void> {
  for (const d of await api.findDownloadsByUrl(url)) dropDownload(d);
}

/** 撤除浏览器下载条目，完成状态同时删除文件。 */
function dropDownload(d: DownloadLike): void {
  handedUrls.delete(d.url);
  if (d.finalUrl) handedUrls.delete(d.finalUrl);
  // 已完成条目的文件一并撤除，取消与记录清除对进行中条目生效
  if (d.state === "complete") api.removeDownloadFile(d.id);
  api.cancelDownload(d.id);
  api.eraseDownload(d.id);
}

/** 监听浏览器下载创建事件并撤除已移交条目。 */
export function installDownloadCancellation(): void {
  api.onDownloadCreated((d) => {
    if (!settings.enabled) return;
    if (![d.finalUrl, d.url].some((u) => u && handedUrls.has(u))) return;
    dropDownload(d);
  });
}

function pruneCandidates(): void {
  const now = Date.now();
  for (const [id, c] of candidates) {
    if (now > c.expires) {
      candidates.delete(id);
      const key = frameKey(c.pending.tabId, c.pending.frameId);
      const list = frameItems.get(key);
      if (list) {
        const i = list.indexOf(id);
        if (i >= 0) list.splice(i, 1);
      }
    }
  }
}


function notice(message: string): void {
  if (noticeTab !== undefined) api.sendMessageToTab(noticeTab, { kind: "notify", message }, 0);
}

ws.onOffline = () => notice(api.i18n("noticeOffline"));
ws.onOversize = () => notice(api.i18n("noticeOversize"));

/** 使用给定连接参数测试引擎可达性与鉴权。 */
export function testConnection(host: string, port: number, token: string): Promise<ConnectionTestResult> {
  return ws.testConnection({ ...DEFAULT_SETTINGS, host, port, token });
}

/** 清理标签页面板、来源帧与提示目标。 */
export function onTabClosed(tabId: number): void {
  for (const key of [...frameItems.keys()]) if (key.startsWith(`${tabId}:`)) frameItems.delete(key);
  removeTabFrames(tabId);
  if (noticeTab === tabId) noticeTab = undefined;
}
