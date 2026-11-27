import { api } from "@platform";
import { getFrame } from "@common/capture/frames";
import { getHeader, stripHash } from "@common/util/net";
import { isMediaExt } from "@common/data/tables";
import type { Pending } from "@common/types/pipeline";
import type { CreateTaskParams } from "@common/types/ws";

interface PageContext {
  referer?: string;
  origin?: string;
  pageTitle?: string;
  pageUrl?: string;
  closeTabId?: number;
}

/** 组装移交任务，补齐来源页、Cookie 与可透传请求头。 */
export async function buildJobFromPending(p: Pending): Promise<CreateTaskParams | null> {
  const ctx = await resolveContext(p);

  const params: CreateTaskParams = { url: p.url };
  if (ctx.referer) params.referer = ctx.referer;
  if (ctx.origin) params.origin = ctx.origin;
  if (ctx.pageTitle) params.pageTitle = ctx.pageTitle;
  if (ctx.pageUrl) params.pageUrl = ctx.pageUrl;
  if (p.fileName) params.fileName = p.fileName;
  if (p.mime) params.mimeType = p.mime;
  if (p.size && p.size > 0) params.size = p.size;

  const headers: Record<string, string> = {};
  for (const h of p.reqHeaders ?? []) {
    if (h.name.toLowerCase().startsWith("x-")) headers[h.name] = h.value;
  }

  const contentType = getHeader(p.reqHeaders, "Content-Type");
  if (contentType) headers["Content-Type"] = contentType;

  if (Object.keys(headers).length) params.headers = headers;

  const jar = await api.getCookies(p.url);
  if (jar.length) params.cookie = jar.map((c) => `${c.name}=${c.value}`).join("; ");

  if (ctx.closeTabId !== undefined && p.effectiveExt && isMediaExt(p.effectiveExt)) {
    api.removeTab(ctx.closeTabId);
  }

  return params;
}

/** 解析来源帧；主框架下载链来自新标签时改查 opener。 */
async function resolveContext(p: Pending): Promise<PageContext> {
  const frame = getFrame(p.tabId, p.frameId);
  const main = getFrame(p.tabId, 0);

  let refererUrl = frame?.url ?? main?.url ?? "";
  let title = frame?.title ?? main?.title ?? "";

  // target=_blank：下载链正是某活动标签地址且由 opener 打开，改用 opener 页作来源
  if (!refererUrl || p.type === "main_frame") {
    const tabs = await api.queryTabs();
    const blank = tabs.find(
      (t) => t.active && (t.url === p.url || t.pendingUrl === p.url) && t.openerTabId !== undefined && t.openerTabId >= 0
    );
    if (blank?.openerTabId !== undefined) {
      const opener = await api.getTab(blank.openerTabId);
      if (opener?.url) {
        refererUrl = opener.url;
        title = opener.title ?? title;
        return finalize(refererUrl, title, blank.id);
      }
    }
  }

  if (!refererUrl) {
    const tab = await api.getTab(p.tabId);
    refererUrl = tab?.url ?? "";
    title = title || tab?.title || "";
  }

  return finalize(refererUrl, title, undefined);
}

function finalize(refererUrl: string, title: string, closeTabId?: number): PageContext {
  const ctx: PageContext = {};
  if (refererUrl) {
    ctx.referer = stripHash(refererUrl);
    ctx.pageUrl = stripHash(refererUrl);
    try {
      ctx.origin = new URL(refererUrl).origin;
    } catch {
      /* 非法地址不填 origin */
    }
  }
  if (title) ctx.pageTitle = title;
  if (closeTabId !== undefined) ctx.closeTabId = closeTabId;
  return ctx;
}
