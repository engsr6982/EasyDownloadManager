import { isForceMime, isLegacyExt, isMediaExt, isSegmentExt, isUrlBlacklisted, isWebResExt } from "@common/data/tables";
import { urlLastSegment } from "@common/util/net";
import type { Pending } from "@common/types/pipeline";

const SNIFFABLE = new Set(["object", "xmlhttprequest", "media", "other"]);

/** 按资源类型、扩展名、MIME 与 disposition 判定自动接管范围。 */
export function isDownload(p: Pending): boolean {
  const ext = p.effectiveExt ?? "";
  if (isUrlBlacklisted(urlLastSegment(p.url))) return false;
  if ((p.mime ?? "").toLowerCase().includes("json")) return false;
  if (ext === "json") return false;

  const isNavFrame = p.type === "main_frame" || p.type === "sub_frame";
  const sniffable = SNIFFABLE.has(p.type);

  if (p.type === "main_frame" && isMediaExt(ext) && !isSegmentExt(ext)) return true;
  if (p.type === "other" && isMediaExt(ext)) return true;
  if ((isNavFrame || !sniffable) && isLegacyExt(ext)) return true;
  if (isNavFrame || p.type === "other") {
    if (p.disposition === "attachment") return true;
    if (isForceMime(p.mime ?? "")) return true;
    if (ext && !isMediaExt(ext) && !isWebResExt(ext)) return true;
  }
  return false;
}
