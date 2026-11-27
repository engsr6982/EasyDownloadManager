import { api } from "@platform";
import { MENU_ID_IMAGE, MENU_ID_LINK } from "@common/api/types";
import type { MenuInfo, TabLike } from "@common/api/types";
import { installFrameRegistry } from "@common/capture/frames";
import { installPipeline } from "@common/capture/pipeline";
import {
  clearPanels,
  handOffExplicit,
  installDownloadCancellation,
  onPanelClick,
  onTabClosed,
  refreshSettings,
  testConnection,
} from "@common/capture/handoff";
import type { Settings } from "@common/settings";
import type { ContentToBackground } from "@common/types/messages";
import type { Pending } from "@common/types/pipeline";

/** 装配后台监听、菜单与消息路由，并在每次唤醒时应用设置。 */
export function startBackground(): void {
  void refreshSettings().then(applyMenus);
  installFrameRegistry();
  installPipeline();
  installDownloadCancellation();
  api.onMenuClicked(onMenuClicked);
  api.onMessage(onMessage);
  api.onTabRemoved(onTabClosed);
}

/** 右键菜单项由总开关与菜单开关共同决定 */
function applyMenus(s: Settings): void {
  if (s.enabled && s.contextMenu) api.createDownloadMenus(api.i18n("menuDownload"));
  else api.removeDownloadMenus();
}

function onMenuClicked(info: MenuInfo, tab: TabLike): void {
  const url =
    info.menuItemId === MENU_ID_IMAGE ? info.srcUrl : info.menuItemId === MENU_ID_LINK ? info.linkUrl : undefined;
  if (!url || tab.id === undefined || !/^https?:/i.test(url)) return;
  const p: Pending = { url, tabId: tab.id, frameId: info.frameId, type: "other", method: "GET" };
  void handOffExplicit(p);
}

/** 路由内容脚本与弹窗消息，仅连接测试保持异步应答通道。 */
function onMessage(
  message: unknown,
  sender: { tab?: TabLike; frameId?: number },
  sendResponse: (r?: unknown) => void
): boolean | void {
  const m = message as ContentToBackground;
  if (!m || typeof m.kind !== "string") return;
  switch (m.kind) {
    case "panel:click": {
      const tabId = sender.tab?.id;
      if (tabId !== undefined) void onPanelClick(tabId, m.id);
      return;
    }
    case "settings:changed":
      void (async () => {
        const s = await refreshSettings();
        applyMenus(s);
        if (!s.enabled || !s.sniffPanel) await clearPanels();
      })();
      return;
    case "testConnection":
      void testConnection(m.host, m.port, m.token).then(sendResponse);
      return true; // 异步应答期间保持消息通道
  }
}
