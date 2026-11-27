// 后台 Service Worker 与内容脚本 / 弹窗之间的运行时消息契约。

/** 面板条目在内容脚本侧的可渲染投影；完整任务数据只留在后台，按 id 解析 */
export interface PanelItemView {
  id: string;
  /** 主标题：文件名或清晰度描述 */
  label: string;
  /** 次级说明：大小、类型等，可空 */
  sub?: string;
  /** 媒体地址，用于在页面中锚定对应元素 */
  url: string;
}

/** 连接测试结果：区分连不上与令牌不对 */
export interface ConnectionTestResult {
  reachable: boolean;
  authenticated: boolean;
}

/** 内容脚本与弹窗发送到后台的消息。 */
export type ContentToBackground =
  /** 面板条目被点击，携带条目 id 由后台解析并移交 */
  | { kind: "panel:click"; id: string }
  /** 弹窗保存设置后通知后台重读并应用变更。 */
  | { kind: "settings:changed" }
  /** 弹窗按当前表单值测试引擎连接与鉴权，返回 ConnectionTestResult */
  | { kind: "testConnection"; host: string; port: number; token: string };

/** 后台发送到内容脚本的消息。 */
export type BackgroundToContent =
  /** 推送/刷新面板条目集合 */
  | { kind: "panel:sync"; items: PanelItemView[] }
  /** 清空面板 */
  | { kind: "panel:clear" }
  /** SPA 换地址后请求内容脚本重扫：刷新 frame 上报并重新定位面板 */
  | { kind: "rescan" }
  /** 一次性提示，如引擎离线 */
  | { kind: "notify"; message: string };

/** 内容脚本上报来源帧。 */
export type PortToBackground = { type: "report"; url: string; title: string };
/** 后台确认端口所属帧。 */
export type BackgroundToPort = { type: "hello"; frameId: number };
