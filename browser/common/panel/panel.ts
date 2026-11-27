import { api } from "@platform";
import { PANEL_FADE_IDLE_MS, PANEL_FADE_OUT_MS, PANEL_RELAYOUT_INTERVAL_MS } from "@common/data/constants";
import type { PanelItemView } from "@common/types/messages";

const MEDIA_SELECTOR = "video,audio,object,embed";
/** 两个锚点在该像素距离内视为同一目标，合并成一个面板 */
const MERGE_DISTANCE = 18;
const Z = "2147483647";

interface Group {
  el: HTMLElement;
  head: HTMLElement;
  list: HTMLElement;
  items: PanelItemView[];
  expanded: boolean;
  dragged: boolean;
  pos: { left: number; top: number };
  dimTimer: ReturnType<typeof setTimeout> | null;
}

let lastClick: { x: number; y: number } | null = null;

/** 安装浮动面板，返回同步、重定位与清理句柄。 */
export function createPanel(onClick: (id: string) => void) {
  const groups = new Map<string, Group>();
  const style = injectStyle();

  const primaryId = (g: Group): string => g.items[0]?.id ?? "";
  const near = (a: { left: number; top: number }, b: { left: number; top: number }) =>
    Math.hypot(a.left - b.left, a.top - b.top) <= MERGE_DISTANCE;
  const sameIds = (a: PanelItemView[], b: PanelItemView[]) =>
    a.length === b.length && a.every((x, i) => x.id === b[i].id);

  function rectFor(item: PanelItemView): { left: number; top: number } {
    const el = findAnchor(item);
    if (!el) return { left: 1, top: 1 };
    const r = el.getBoundingClientRect();
    return { left: Math.round(r.left) + 1, top: Math.round(r.top) - 23 };
  }

  function decorate(g: Group): void {
    g.head.textContent = "";
    g.list.textContent = "";
    const caption = (it: PanelItemView) => it.label + (it.sub ? ` (${it.sub})` : "");

    if (g.items.length === 1) {
      const it = g.items[0];
      g.head.textContent = caption(it);
      g.head.title = api.i18n("panelTitle");
      g.head.onclick = () => onClick(it.id);
      g.list.style.display = "none";
      return;
    }

    g.head.textContent = `${g.items.length} ${api.i18n("panelFiles")}`;
    g.head.title = g.expanded ? api.i18n("panelCollapse") : api.i18n("panelExpand");
    g.head.onclick = () => {
      g.expanded = !g.expanded;
      g.list.style.display = g.expanded ? "block" : "none";
      g.head.title = g.expanded ? api.i18n("panelCollapse") : api.i18n("panelExpand");
    };
    for (const it of g.items) {
      const row = document.createElement("button");
      row.className = "edm-row";
      row.textContent = caption(it);
      row.title = api.i18n("panelTitle");
      row.onclick = () => onClick(it.id);
      g.list.appendChild(row);
    }
    g.list.style.display = g.expanded ? "block" : "none";
  }

  function buildGroup(item: PanelItemView, pos: { left: number; top: number }): Group {
    const el = document.createElement("div");
    el.setAttribute("data-edm-panel", "");
    const head = document.createElement("button");
    head.className = "edm-head";
    const list = document.createElement("div");
    list.className = "edm-list";
    el.appendChild(head);
    el.appendChild(list);
    const g: Group = { el, head, list, items: [item], expanded: false, dragged: false, pos, dimTimer: null };
    decorate(g);
    enableDrag(g);
    el.addEventListener("mouseenter", () => wake(g));
    el.addEventListener("mouseleave", () => scheduleDim(g, PANEL_FADE_OUT_MS));
    el.addEventListener("pointerdown", (e) => e.stopPropagation());
    document.body.appendChild(el);
    position(g);
    wake(g);
    return g;
  }

  function enableDrag(g: Group): void {
    g.head.addEventListener("pointerdown", (down) => {
      const startX = down.clientX - g.pos.left;
      const startY = down.clientY - g.pos.top;
      const move = (ev: PointerEvent) => {
        g.dragged = true;
        g.pos = { left: ev.clientX - startX, top: ev.clientY - startY };
        position(g);
      };
      const up = () => {
        document.removeEventListener("pointermove", move);
        document.removeEventListener("pointerup", up);
      };
      document.addEventListener("pointermove", move);
      document.addEventListener("pointerup", up);
    });
  }

  function position(g: Group): void {
    g.el.style.left = `${g.pos.left}px`;
    g.el.style.top = `${g.pos.top}px`;
  }

  function wake(g: Group): void {
    if (g.dimTimer) clearTimeout(g.dimTimer);
    g.el.style.opacity = "1";
    g.dimTimer = setTimeout(() => dim(g), PANEL_FADE_IDLE_MS);
  }

  function scheduleDim(g: Group, delay: number): void {
    if (g.dimTimer) clearTimeout(g.dimTimer);
    g.dimTimer = setTimeout(() => dim(g), delay);
  }

  function dim(g: Group): void {
    g.el.style.opacity = "0.55";
  }

  function drop(g: Group): void {
    if (g.dimTimer) clearTimeout(g.dimTimer);
    g.el.remove();
  }

  /** 全量刷新：按 id 归位既有面板，就近合并新条目，多余面板移除 */
  function sync(items: PanelItemView[]): void {
    const pool = [...groups.values()];
    const buckets = new Map<Group, PanelItemView[]>();

    for (const item of items) {
      let owner = pool.find((g) => g.items.some((x) => x.id === item.id));
      if (!owner) {
        const rect = rectFor(item);
        owner = pool.find((g) => !buckets.has(g) && !g.dragged && near(g.pos, rect));
        if (!owner) {
          owner = buildGroup(item, rect);
          pool.push(owner);
        }
      }
      buckets.set(owner, [...(buckets.get(owner) ?? []), item]);
    }

    for (const g of pool) if (!buckets.has(g)) drop(g);

    groups.clear();
    for (const [g, list] of buckets) {
      if (!sameIds(g.items, list)) {
        g.items = list;
        decorate(g);
      }
      groups.set(primaryId(g), g);
    }
  }

  /** 锚元素可能随布局变化移动或被替换，按当前锚点重新定位未拖动过的面板 */
  function relayout(): void {
    for (const g of groups.values()) {
      if (g.dragged || !g.items.length) continue;
      g.pos = rectFor(g.items[0]);
      position(g);
    }
  }

  const relayoutTimer = setInterval(relayout, PANEL_RELAYOUT_INTERVAL_MS);

  const onDocClick = (e: MouseEvent) => {
    lastClick = { x: e.clientX, y: e.clientY };
    for (const g of groups.values()) {
      if (g.expanded && !g.el.contains(e.target as Node)) {
        g.expanded = false;
        g.list.style.display = "none";
        g.head.title = api.i18n("panelExpand");
      }
    }
  };
  document.addEventListener("click", onDocClick, true);

  return {
    sync,
    relayout,
    /** 移除全部面板、计时器与文档监听。 */
    clear(): void {
      for (const g of groups.values()) drop(g);
      groups.clear();
      clearInterval(relayoutTimer);
      document.removeEventListener("click", onDocClick, true);
      style.remove();
    },
  };
}

function isMedia(el: Element | null): boolean {
  return !!el && ["VIDEO", "AUDIO", "OBJECT", "EMBED"].includes(el.tagName);
}

/** 依次按源地址、聚焦媒体、最近点击元素、最大可见播放器查找锚点。 */
function findAnchor(item: PanelItemView): Element | null {
  const media = Array.from(document.querySelectorAll(MEDIA_SELECTOR));
  const target = normalize(item.url);
  const pageUrl = normalize(location.href);
  for (const m of media) {
    for (const src of sourcesOf(m)) {
      const n = normalize(src);
      if (n === target || n === pageUrl) return m;
    }
  }
  if (isMedia(document.activeElement)) return document.activeElement;
  if (lastClick) {
    const el = document.elementFromPoint(lastClick.x, lastClick.y);
    if (isMedia(el)) return el;
    const inside = el?.closest(MEDIA_SELECTOR) ?? null;
    if (inside) return inside;
  }
  let best: Element | null = null;
  let bestArea = 0;
  for (const m of media) {
    const r = m.getBoundingClientRect();
    if (r.width < 2 || r.height < 2) continue;
    const ratio = r.width / r.height;
    if (ratio < 0.71 || ratio > 3) continue;
    const area = r.width * r.height;
    if (area > bestArea) {
      bestArea = area;
      best = m;
    }
  }
  return best;
}

function sourcesOf(el: Element): string[] {
  const out: string[] = [];
  const a = el as HTMLMediaElement & { data?: string };
  out.push(a.currentSrc ?? "", a.src ?? "", a.data ?? "");
  el.querySelectorAll("source").forEach((s) => out.push((s as HTMLSourceElement).src ?? ""));
  return out.filter(Boolean);
}

function normalize(u: string): string {
  const i = u.indexOf("#");
  return i < 0 ? u : u.slice(0, i);
}

function injectStyle(): HTMLStyleElement {
  const el = document.createElement("style");
  el.textContent =
    `[data-edm-panel]{all:revert;position:fixed;z-index:${Z};font:13px/1.4 system-ui,sans-serif;` +
    `color:#fff;opacity:1;transition:opacity .3s;border-radius:8px;background:#1b1b1bf0;box-shadow:0 4px 18px #0006;` +
    `min-width:120px;max-width:320px}` +
    `[data-edm-panel] .edm-head{all:revert;display:block;width:100%;text-align:left;cursor:move;color:#fff;background:#2b6df0;` +
    `border:0;border-radius:8px;padding:6px 10px;font:600 13px/1.4 system-ui,sans-serif;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}` +
    `[data-edm-panel] .edm-list{background:#262626f5;border-radius:0 0 8px 8px}` +
    `[data-edm-panel] .edm-row{all:revert;display:block;width:100%;text-align:left;cursor:pointer;color:#fff;background:transparent;border:0;` +
    `border-top:1px solid #ffffff22;padding:5px 10px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}` +
    `[data-edm-panel] .edm-row:hover{background:#2b6df0}`;
  document.documentElement.appendChild(el);
  return el;
}
