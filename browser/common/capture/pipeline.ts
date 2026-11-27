import { api } from "@platform";
import { getHeader, parseHeaderParam, protocolOf } from "@common/util/net";
import { inferFileName } from "@common/util/filename";
import { isDownload } from "@common/capture/judge";
import { sniffCandidate } from "@common/capture/sniff";
import { handOff, pushPanelCandidate } from "@common/capture/handoff";
import type { Pending } from "@common/types/pipeline";
import type { RequestObservation, ResourceType } from "@common/api/types";

const OBSERVED: ResourceType[] = ["object", "xmlhttprequest", "media", "other", "main_frame", "sub_frame", "image"];

/** requestId 对应整条重定向链的观察态。 */
const pending = new Map<string, Pending>();

const REDIRECTS = new Set([301, 302, 303, 307, 308]);

/** 安装只读请求观察流水线，按 requestId 聚合重定向链。 */
export function installPipeline(): void {
  api.onBeforeRequest((ev) => {
    if (ev.tabId < 0 || ev.frameId < 0) return; // 非页面发起的请求不观察
    const p = pending.get(ev.requestId) ?? newPending(ev);
    pending.set(ev.requestId, p);
  }, OBSERVED);

  api.onBeforeSendHeaders((ev) => {
    const p = pending.get(ev.requestId);
    if (p && ev.requestHeaders) p.reqHeaders = ev.requestHeaders;
  }, OBSERVED);

  api.onHeadersReceived((ev) => {
    const p = pending.get(ev.requestId);
    if (!p) return;
    p.url = ev.url; // 关键：写回重定向后的最终地址
    if (ev.status !== undefined && REDIRECTS.has(ev.status)) return; // 302 等：pending 跟随下一跳
    if (!["http", "https"].includes(protocolOf(ev.url))) {
      pending.delete(ev.requestId);
      return;
    }
    if (ev.method.toUpperCase() !== "GET") {
      // 引擎按 GET 回源，非 GET 请求不可复现
      pending.delete(ev.requestId);
      return;
    }
    p.respHeaders = ev.responseHeaders ?? [];
    const mime = getHeader(p.respHeaders, "Content-Type").split(";")[0].trim().toLowerCase();
    if (ev.type === "image" && mime.startsWith("image/")) {
      pending.delete(ev.requestId);
      return;
    }
    if (ev.status !== undefined && ![200, 206].includes(ev.status)) {
      pending.delete(ev.requestId);
      return;
    }

    let total = Number(getHeader(p.respHeaders, "Content-Length"));
    const cr = /^bytes [0-9]+-[0-9]+\/([0-9]+)$/.exec(getHeader(p.respHeaders, "Content-Range"));
    if (cr) total = Number(cr[1]);
    if (!(total > 0)) total = -1;

    fillNameAndMeta(p, mime, total);
    pending.delete(ev.requestId);

    if (isDownload(p)) void handOff(p);
    else {
      const cand = sniffCandidate(p);
      if (cand) pushPanelCandidate(p.tabId, p, cand.label, cand.sub);
    }
  }, OBSERVED);

  api.onCompleted((ev) => pending.delete(ev.requestId), OBSERVED);
  api.onErrorOccurred((ev) => pending.delete(ev.requestId), OBSERVED);
}

function newPending(ev: RequestObservation): Pending {
  return {
    url: ev.url,
    tabId: ev.tabId,
    frameId: ev.frameId,
    type: ev.type,
    method: ev.method.toUpperCase(),
  };
}

function fillNameAndMeta(p: Pending, mime: string, total: number): void {
  const dispositionValue = getHeader(p.respHeaders, "Content-Disposition");
  const contentType = getHeader(p.respHeaders, "Content-Type");
  const dispName = parseHeaderParam(dispositionValue, "filename");
  const ctName = parseHeaderParam(contentType, "name");
  const { fileName, ext } = inferFileName({
    url: p.url,
    contentType,
    contentDisposition: dispositionValue,
    dispositionFilename: dispName,
    contentName: ctName,
  });
  p.fileName = fileName;
  p.effectiveExt = ext;
  p.mime = mime;
  p.size = total;
  if (/attachment/i.test(dispositionValue)) p.disposition = "attachment";
  else if (/inline/i.test(dispositionValue)) p.disposition = "inline";
}
