import type { Header } from "@common/types/http";

/** 大小写不敏感地取头值，返回最后一个匹配 */
export function getHeader(headers: Header[] | undefined, name: string): string {
  if (!headers) return "";
  const lower = name.toLowerCase();
  let value = "";
  for (const h of headers) {
    if (h.name.toLowerCase() === lower) value = h.value;
  }
  return value;
}

/** 返回 URL 的 scheme 并转小写，无 scheme 时返回空串。 */
export function protocolOf(url: string): string {
  const i = url.indexOf(":");
  return i < 0 ? "" : url.slice(0, i).toLowerCase();
}

/** 去掉 #hash 部分，供 Referer 使用 */
export function stripHash(url: string): string {
  const i = url.indexOf("#");
  return i < 0 ? url : url.slice(0, i);
}

/** URL 路径末段（不含 query/fragment），用于文件名默认推断 */
export function urlLastSegment(url: string): string {
  try {
    const u = new URL(url);
    const seg = u.pathname.split("/").filter(Boolean).pop() ?? "";
    return decodeURIComponent(seg);
  } catch {
    const noHash = stripHash(url).split("#")[0].split("?")[0];
    return decodeURIComponent(noHash.split("/").pop() ?? "");
  }
}

/** 解析头参数，支持引号值与 RFC2231 ext-value。 */
export function parseHeaderParam(headerValue: string, param: string): string {
  if (!headerValue) return "";
  const lower = headerValue.toLowerCase();
  const extIdx = lower.indexOf(`${param.toLowerCase()}*=`);
  if (extIdx >= 0) {
    const decoded = decodeRfc2231(takeParam(headerValue, extIdx + param.length + 1));
    if (decoded) return decoded;
  }
  const idx = findParam(headerValue, param);
  if (idx < 0) return "";
  return unquote(takeParam(headerValue, idx));
}

function findParam(headerValue: string, param: string): number {
  const lower = headerValue.toLowerCase();
  const needle = `${param.toLowerCase()}=`;
  let from = 0;
  for (;;) {
    const i = lower.indexOf(needle, from);
    if (i < 0) return -1;
    // 排除 xxxname= 这种后缀匹配：前一个字符须是分隔符或串首
    if (i === 0 || /[\s;"]/.test(headerValue[i - 1])) return i + needle.length;
    from = i + 1;
  }
}

/** 从 `value;...` 或 `value` 中取到下一个分号前的片段 */
function takeParam(headerValue: string, start: number): string {
  let i = start;
  // ext-value 形如 UTF-8''%E4%B8%AD 三段式，整体到分号为止
  const rest = headerValue.slice(i);
  const semi = findUnquotedSemi(rest);
  return (semi < 0 ? rest : rest.slice(0, semi)).trim();
}

function findUnquotedSemi(s: string): number {
  let inQuote = false;
  for (let i = 0; i < s.length; i++) {
    if (s[i] === '"') inQuote = !inQuote;
    else if (s[i] === ";" && !inQuote) return i;
  }
  return -1;
}

function unquote(v: string): string {
  if (v.length >= 2 && v.startsWith('"') && v.endsWith('"')) return v.slice(1, -1);
  return v;
}

/** ext-value: charset "'" lang "'" percent-encoded */
function decodeRfc2231(v: string): string {
  const parts = v.split("'");
  if (parts.length >= 3) {
    const charset = parts[0].toLowerCase();
    const encoded = parts.slice(2).join("'");
    if (charset === "utf-8" || charset === "utf8") return safeDecodeURIComponent(encoded);
    if (charset === "iso-8859-1" || charset === "latin1") return safeDecodeLatin1(encoded);
    return safeDecodeURIComponent(encoded);
  }
  return safeDecodeURIComponent(v);
}

function safeDecodeURIComponent(s: string): string {
  try {
    return decodeURIComponent(s);
  } catch {
    return s;
  }
}

/** 百分号解码后取低八位，转换 Latin-1 字符。 */
function safeDecodeLatin1(s: string): string {
  const decoded = safeDecodeURIComponent(s);
  let out = "";
  for (let i = 0; i < decoded.length; i++) out += String.fromCharCode(decoded.charCodeAt(i) & 0xff);
  return out;
}
