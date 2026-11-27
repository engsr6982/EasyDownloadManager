import { mimeToExt } from "@common/data/tables";
import { urlLastSegment } from "@common/util/net";

/** 取文件名的扩展名（小写、无点）；无扩展名返回空串 */
function extensionOf(name: string): string {
  const dot = name.lastIndexOf(".");
  if (dot <= 0 || dot === name.length - 1) return "";
  const ext = name.slice(dot + 1);
  return /^[A-Za-z0-9]{1,8}$/.test(ext) ? ext.toLowerCase() : "";
}

/** 去掉文件名末尾的扩展名 */
function stripExtension(name: string): string {
  const dot = name.lastIndexOf(".");
  return dot > 0 ? name.slice(0, dot) : name;
}

/** 文件名推断使用的 URL 与响应头来源。 */
interface NameSources {
  url: string;
  /** 响应 Content-Type 头（可含参数） */
  contentType: string;
  /** 响应 Content-Disposition 头 */
  contentDisposition: string;
  /** Content-Disposition 的 filename 参数值（已解码） */
  dispositionFilename: string;
  /** Content-Type 的 name 参数值（已解码） */
  contentName: string;
}

/** 按 MIME、响应头名称、URL 末段的优先级推断文件名与扩展名。 */
export function inferFileName(s: NameSources): { fileName: string; ext: string } {
  const mimePart = s.contentType.split(";")[0].trim().toLowerCase();
  const extFromMime = mimeToExt(mimePart) ?? "";
  const urlName = urlLastSegment(s.url);
  const nameFromHeader = s.dispositionFilename || s.contentName || "";

  const extFromHeaderName = nameFromHeader ? extensionOf(nameFromHeader) : "";
  const extFromUrl = extensionOf(urlName);

  const ext = extFromMime || extFromHeaderName || extFromUrl;

  // 名称来源：优先 disposition/content-type 给出的文件名，其次 URL 末段
  const base = nameFromHeader || urlName;
  const stem = stripExtension(base);
  const fileName = ext ? (stem ? `${stem}.${ext}` : `file.${ext}`) : base || "download";

  return { fileName: sanitize(fileName), ext };
}

/** 去除路径分隔与控制字符，保留可见字符 */
function sanitize(name: string): string {
  return name.replace(/[\\/:*?"<>|\u0000-\u001f]/g, "_").trim() || "download";
}
