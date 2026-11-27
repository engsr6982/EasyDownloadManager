import type { Header } from "@common/types/http";

/** 贯穿一条重定向链的观察态。requestId 作 key 存在 pending 表。 */
export interface Pending {
  url: string;
  tabId: number;
  frameId: number;
  type: string;
  method: string;
  reqHeaders?: Header[];
  respHeaders?: Header[];
  /** 判定用的最终扩展名（小写、无点） */
  effectiveExt?: string;
  /** 推断出的文件名（含扩展名） */
  fileName?: string;
  /** 响应 Content-Type（首段） */
  mime?: string;
  /** Content-Disposition 类型：attachment / inline / 空 */
  disposition?: string;
  /** 期望总字节数，未知为 -1 */
  size?: number;
}
