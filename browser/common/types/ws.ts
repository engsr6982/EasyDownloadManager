// WebSocket 协议类型，对应 docs/websocket.md。文本帧承载 UTF-8 JSON，字段名区分大小写。

/** 成功响应信封。 */
export interface SuccessEnvelope<T> {
  id: number;
  ok: true;
  result: T;
}

/** 失败响应信封 */
export interface ErrorEnvelope {
  id: number;
  ok: false;
  error: string;
}

/** 事件信封：广播，无 id */
export interface EventEnvelope<T> {
  event: string;
  data: T;
}

/** 服务端响应信封，按 ok 判别成功或失败。 */
export type ResponseEnvelope<T> = SuccessEnvelope<T> | ErrorEnvelope;

/** 移交任务字段，扩展端产生的全部接力材料都落在此处 */
export interface CreateTaskParams {
  url: string;
  referer?: string;
  origin?: string;
  cookie?: string;
  headers?: Record<string, string>;
  fileName?: string;
  mimeType?: string;
  size?: number;
  pageUrl?: string;
  pageTitle?: string;
}

/** 建任务结果。 */
export interface CreateTaskResult {
  taskId: number;
}

/** 鉴权结果。 */
export interface AuthResult {
  authenticated: boolean;
}

/** 服务端任意入站帧的判别联合 */
export type InboundFrame =
  | ResponseEnvelope<AuthResult>
  | ResponseEnvelope<CreateTaskResult>
  | EventEnvelope<unknown>;
