import { isAdServer, isMediaExt, isSegmentExt, isSubtitleExt } from "@common/data/tables";
import { MIN_SNIFF_BYTES, MAX_ASF_BYTES } from "@common/data/constants";
import { getHeader } from "@common/util/net";
import type { Pending } from "@common/types/pipeline";

interface SniffResult {
  label: string;
  sub: string;
}

/** 判定可直下的单文件媒体与字幕候选，排除 HLS 分段列表。 */
export function sniffCandidate(p: Pending): SniffResult | null {
  const ext = p.effectiveExt ?? "";
  if (!ext) return null;
  if (ext === "m3u8") return null;

  const subtitle = isSubtitleExt(ext);
  const isMedia = isMediaExt(ext);
  if (!subtitle && !isMedia) return null;
  if (isSegmentExt(ext)) return null;

  if (!subtitle) {
    const typeOk = p.type === "object" || p.type === "xmlhttprequest" || p.type === "media" || p.type === "other";
    if (!typeOk) return null;
    if (p.size !== undefined && p.size > 0 && p.size <= MIN_SNIFF_BYTES) return null;
    if (ext === "asf" && p.size !== undefined && p.size > 0 && p.size <= MAX_ASF_BYTES) return null;
    if (isAdServer(getHeader(p.respHeaders, "Server"))) return null;
  }

  return {
    label: p.fileName || ext,
    sub: formatSize(p.size ?? -1),
  };
}

function formatSize(bytes: number): string {
  if (bytes <= 0) return "";
  const units = ["B", "KB", "MB", "GB"];
  let v = bytes;
  let i = 0;
  while (v >= 1024 && i < units.length - 1) {
    v /= 1024;
    i++;
  }
  return `${v >= 10 || i === 0 ? Math.round(v) : v.toFixed(1)} ${units[i]}`;
}
