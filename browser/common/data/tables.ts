// 判定用的扩展名表、MIME 映射表与 URL/响应头黑名单。判定数据集中于本文件，逻辑代码不携带具体条目。

/** 媒体容器扩展名。归档与可执行文件不在此表，由判定规则的未知扩展名分支处理。 */
const MEDIA = [
  "flv", "swf", "mp3", "mp4", "m4v", "f4f", "f4v", "m4a", "mpg", "mpeg", "mpeg4", "mpe", "avi", "wmv", "wma",
  "wav", "wave", "asf", "rm", "ram", "ogg", "ogv", "ogm", "oga", "mov", "mid", "midi", "3gp", "3gpp", "qt",
  "webm", "ts", "mkv", "aac", "mp2t", "mpegts", "rmvb", "vtt", "srt",
];

/** 网页资源扩展名：页面脚本、样式与图片，默认不接管。 */
const WEB_RES = [
  "htm", "html", "mht", "mhtml", "shtml", "shtm", "xht", "xhtm", "xhtml", "xml", "txt", "css", "js", "json",
  "gif", "ico", "jpeg", "jpg", "png", "webp", "bmp", "svg", "tif", "tiff", "pdf", "php", "asp", "aspx", "eot",
  "ttf", "wof", "woff", "woff2", "msg", "chn", "pem", "br", "otf", "acz", "azc", "cgi", "tpl", "osd", "m3u8", "do",
];

/** 流媒体分片扩展名：HLS 等分段文件，地址栏直达时排除。 */
const SEGMENT = ["f4f", "mpegts", "ts", "mp2t"];

/** 老式媒体扩展名，仅导航/非嗅探类型使用。 */
const LEGACY = ["flv", "avi", "mpg", "mpe", "wmv", "qt", "mov", "rm", "ram", "wma", "mid", "midi", "aac", "mkv", "rmvb"];

function toMatcher(list: string[]): (ext: string) => boolean {
  const set = new Set(list.map((e) => e.toLowerCase()));
  return (ext: string) => set.has(ext.toLowerCase());
}

/** 判断扩展名是否属于媒体容器表。 */
export const isMediaExt = toMatcher(MEDIA);
/** 判断扩展名是否属于网页资源表。 */
export const isWebResExt = toMatcher(WEB_RES);
/** 判断扩展名是否属于流媒体分片表。 */
export const isSegmentExt = toMatcher(SEGMENT);
/** 判断扩展名是否属于老式媒体表。 */
export const isLegacyExt = toMatcher(LEGACY);

/** 字幕扩展名（面板以 normal 模式移交；嗅探时不受请求类型限制）。 */
export const isSubtitleExt = toMatcher(["vtt", "srt"]);

/** 带这些 MIME 的响应即使扩展名是网页资源也强制视为下载。 */
const FORCE_MIME = [
  "application/octet-stream",
  "binary/octet-stream",
  "application/x-msdownload",
  "application/x-download",
  "application/download",
  "application/force-download",
  "application/unknown",
  "application/x-unknown",
  "application/x-oleobject",
  "application/dat",
  "application/msword",
  "application/zip",
];
const forceMimeSet = new Set(FORCE_MIME);
/** 判断 MIME 是否属于强制下载类型。 */
export function isForceMime(mime: string): boolean {
  const key = mime.split(";")[0].trim().toLowerCase();
  return forceMimeSet.has(key);
}

/** 广告服务器特征：响应 Server 命中则不进入嗅探面板。 */
const AD_SERVERS = ["dclk-adsvr", "doubleclick", "adsystem", "adnxs", "rubicon", "taboola"];
/** 判断响应 Server 是否包含广告服务特征。 */
export function isAdServer(serverHeader: string): boolean {
  const v = serverHeader.toLowerCase();
  return AD_SERVERS.some((t) => v.includes(t));
}

/** URL 末段文件名黑名单：命中永不自动下载（仍可进嗅探）。 */
export function isUrlBlacklisted(lastSegment: string): boolean {
  const f = lastSegment.toLowerCase();
  if (f.includes("manif")) return true;
  if (f === "favicon.ico") return true;
  if (f.includes("pem.msg")) return true;
  if (f.endsWith(".wasm")) return true;
  if (f.includes(".json")) return true;
  return false;
}

/** MIME 与扩展名映射表，覆盖 video/audio/image/application/text。 */
const MIME_TO_EXT: Record<string, string> = {
  // video
  "video/mp4": "mp4",
  "video/x-m4v": "m4v",
  "video/webm": "webm",
  "video/x-matroska": "mkv",
  "video/quicktime": "mov",
  "video/x-msvideo": "avi",
  "video/x-ms-wmv": "wmv",
  "video/x-ms-asf": "asf",
  "video/mpeg": "mpg",
  "video/mp2t": "ts",
  "video/x-flv": "flv",
  "video/3gpp": "3gp",
  "video/ogg": "ogv",
  // audio
  "audio/mpeg": "mp3",
  "audio/mp4": "m4a",
  "audio/x-m4a": "m4a",
  "audio/wav": "wav",
  "audio/x-wav": "wav",
  "audio/wave": "wave",
  "audio/ogg": "ogg",
  "audio/aac": "aac",
  "audio/x-aac": "aac",
  "audio/flac": "flac",
  "audio/x-ms-wma": "wma",
  "audio/midi": "mid",
  "audio/mpegurl": "m3u8",
  "application/mpegurl": "m3u8",
  "application/x-mpegurl": "m3u8",
  "audio/x-pn-realaudio": "ram",
  // image
  "image/png": "png",
  "image/jpeg": "jpg",
  "image/gif": "gif",
  "image/webp": "webp",
  "image/bmp": "bmp",
  "image/svg+xml": "svg",
  "image/tiff": "tiff",
  "image/vnd.microsoft.icon": "ico",
  "image/x-icon": "ico",
  // text / subtitle
  "text/plain": "txt",
  "text/html": "html",
  "text/css": "css",
  "text/javascript": "js",
  "application/javascript": "js",
  "application/json": "json",
  "text/vtt": "vtt",
  "application/x-subrip": "srt",
  "application/x-srt": "srt",
  // application / archive
  "application/pdf": "pdf",
  "application/zip": "zip",
  "application/x-7z-compressed": "7z",
  "application/x-rar-compressed": "rar",
  "application/gzip": "gz",
  "application/x-tar": "tar",
  "application/x-msdownload": "exe",
  "application/octet-stream": "bin",
  "application/x-bittorrent": "torrent",
  "application/epub+zip": "epub",
};

/** 按 MIME 首段查询扩展名，未知类型返回 undefined。 */
export function mimeToExt(mime: string): string | undefined {
  return MIME_TO_EXT[mime.split(";")[0].trim().toLowerCase()];
}
