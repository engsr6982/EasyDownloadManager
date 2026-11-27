import { CONNECTION_TEST_TIMEOUT_MS, MAX_PAYLOAD_BYTES, READY_WAIT_MS } from "@common/data/constants";
import { wsUrl, type Settings } from "@common/settings";
import type { ConnectionTestResult } from "@common/types/messages";
import type { CreateTaskParams, InboundFrame } from "@common/types/ws";

const CONNECT_TIMEOUT_MS = 4000;

/** 按需连接本机引擎，合并并发建链并顺序发送鉴权后的任务。 */
export class WsClient {
  private settings: Settings | null = null;
  private ws: WebSocket | null = null;
  private authed = false;
  private reqId = 0;
  private connecting: Promise<void> | null = null;
  private queue: CreateTaskParams[] = [];

  /** 断线且有任务未能送达时回调（后台据此提示当前标签页） */
  onOffline: (() => void) | null = null;
  /** 任务超长被拒时回调 */
  onOversize: (() => void) | null = null;

  /** 更新连接配置并关闭旧连接。 */
  configure(s: Settings): void {
    this.settings = s;
    if (this.ws) {
      try {
        this.ws.close();
      } catch {
        /* 忽略 */
      }
      this.ws = null;
      this.authed = false;
      this.connecting = null;
    }
  }

  /** 弹窗按当前表单值测试连接：区分端口不通与令牌不匹配，不改变主连接状态 */
  testConnection(s: Settings): Promise<ConnectionTestResult> {
    return new Promise<ConnectionTestResult>((resolve) => {
      let reachable = false;
      let settled = false;
      let ws: WebSocket;
      const finish = (r: ConnectionTestResult) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        try {
          ws.close();
        } catch {
          /* 忽略 */
        }
        resolve(r);
      };
      try {
        ws = new WebSocket(wsUrl(s));
      } catch {
        finish({ reachable: false, authenticated: false });
        return;
      }
      const timer = setTimeout(() => finish({ reachable, authenticated: false }), CONNECTION_TEST_TIMEOUT_MS);
      ws.onopen = () => {
        reachable = true;
        try {
          ws.send(JSON.stringify({ id: 1, method: "auth", params: { token: s.token } }));
        } catch {
          finish({ reachable, authenticated: false });
        }
      };
      ws.onmessage = (ev) => {
        let frame: { id?: number; ok?: boolean; result?: { authenticated?: boolean } };
        try {
          frame = JSON.parse(String(ev.data));
        } catch {
          return;
        }
        if (frame.id === 1) finish({ reachable: true, authenticated: !!frame.ok && frame.result?.authenticated === true });
      };
      ws.onclose = () => finish({ reachable, authenticated: false });
      ws.onerror = () => {
        /* onclose 紧随其后 */
      };
    });
  }

  /** 表示连接已打开且鉴权通过。 */
  get ready(): boolean {
    return !!this.ws && this.ws.readyState === WebSocket.OPEN && this.authed;
  }

  /** 移交一笔任务；返回是否已即时送达引擎（true 才允许取消浏览器下载） */
  async submit(params: CreateTaskParams): Promise<boolean> {
    if (!this.ready) await this.ensureConnected();
    if (!this.ready) {
      this.queue.push(params);
      const sent = await this.waitReadyThenFlush();
      if (!sent.has(params)) this.onOffline?.();
      return sent.has(params);
    }
    const ok = await this.sendNow(params);
    if (!ok) this.onOffline?.();
    return ok;
  }

  /** 等待共享连接就绪，首个调用者取走队列并顺序发送。 */
  private async waitReadyThenFlush(): Promise<Set<CreateTaskParams>> {
    const sent = new Set<CreateTaskParams>();
    const start = Date.now();
    while (Date.now() - start < READY_WAIT_MS) {
      await sleep(120);
      if (this.ready) break;
    }
    if (this.ready) {
      const pending = this.queue.splice(0, this.queue.length);
      for (const p of pending) {
        if (await this.sendNow(p)) sent.add(p);
      }
    } else {
      this.queue.length = 0;
    }
    return sent;
  }

  /** 合并并发建链，鉴权失败、关闭或超时均正常结束。 */
  private ensureConnected(): Promise<void> {
    if (this.connecting) return this.connecting;
    if (!this.settings) return Promise.resolve();
    const url = wsUrl(this.settings);
    this.connecting = new Promise<void>((resolve) => {
      let settled = false;
      const done = () => {
        if (settled) return;
        settled = true;
        this.connecting = null;
        resolve();
      };
      let ws: WebSocket;
      try {
        ws = new WebSocket(url);
      } catch {
        done();
        return;
      }
      ws.onopen = () => {
        this.reqId = 1;
        this.sendRaw({ id: this.reqId, method: "auth", params: { token: this.settings!.token } });
      };
      ws.onmessage = (ev) => {
        let frame: InboundFrame;
        try {
          frame = JSON.parse(String(ev.data)) as InboundFrame;
        } catch {
          return;
        }
        if ("id" in frame && frame.id === this.reqId && !this.authed) {
          const r = frame as { ok?: boolean; result?: { authenticated?: boolean } };
          this.authed = !!r.ok && r.result?.authenticated === true;
        }
        if (this.authed) done();
      };
      ws.onclose = () => {
        this.ws = null;
        this.authed = false;
        done();
      };
      ws.onerror = () => {
        /* onclose 紧随其后，统一在 onclose 收尾 */
      };
      this.ws = ws;
      setTimeout(done, CONNECT_TIMEOUT_MS);
    });
    return this.connecting;
  }

  /** 补全任务元数据并检查载荷上限，返回值仅表示本地发送成功。 */
  private async sendNow(params: CreateTaskParams): Promise<boolean> {
    const enriched = await this.enrich(params);
    const payload = JSON.stringify({ id: 0, method: "createTask", params: enriched });
    if (new Blob([payload]).size > MAX_PAYLOAD_BYTES) {
      this.onOversize?.();
      return false;
    }
    this.reqId += 1;
    return this.sendRaw({ id: this.reqId, method: "createTask", params: enriched });
  }

  /** 向已打开的 WebSocket 发送 JSON，失败时返回 false。 */
  private sendRaw(obj: unknown): boolean {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return false;
    try {
      this.ws.send(JSON.stringify(obj));
      return true;
    } catch {
      return false;
    }
  }

  /** 缺 size 或 mime 时带浏览器登录态发 HEAD 补全，失败时保留已有字段。 */
  private async enrich(params: CreateTaskParams): Promise<CreateTaskParams> {
    if (params.size && params.size > 0 && params.mimeType) return params;
    try {
      const r = await fetch(params.url, { method: "HEAD", credentials: "include" });
      if (r.ok) {
        if (!params.size || params.size <= 0) {
          const len = Number(r.headers.get("content-length"));
          if (Number.isFinite(len) && len > 0) params.size = len;
        }
        if (!params.mimeType) {
          const ct = r.headers.get("content-type");
          if (ct) params.mimeType = ct;
        }
      }
    } catch {
      /* 保留已有字段 */
    }
    return params;
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}
