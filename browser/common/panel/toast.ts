// 页面内一次性提示条：用于引擎离线等无法放进面板的短消息。

const SHOW_MS = 5000;
const FADE_MS = 600;

let el: HTMLElement | null = null;
let hideTimer: ReturnType<typeof setTimeout> | null = null;

/** 在页面底部显示一次性提示，并重置隐藏计时。 */
export function showToast(message: string): void {
  if (!message) return;
  if (!el) {
    el = document.createElement("div");
    el.style.cssText = [
      "all: revert",
      "position: fixed",
      "left: 50%",
      "bottom: 28px",
      "transform: translateX(-50%)",
      "z-index: 2147483646",
      "max-width: 520px",
      "padding: 10px 16px",
      "border-radius: 6px",
      "background: #1f1f1f",
      "color: #ffffff",
      "font: 13px/1.5 system-ui, -apple-system, Segoe UI, sans-serif",
      "box-shadow: 0 4px 16px rgba(0,0,0,.32)",
      "opacity: 0",
      "transition: opacity .18s ease",
      "pointer-events: none",
    ].join(";");
    document.documentElement.appendChild(el);
  }
  el.textContent = message;
  const node = el;
  requestAnimationFrame(() => {
    node.style.opacity = "1";
  });
  if (hideTimer) clearTimeout(hideTimer);
  hideTimer = setTimeout(() => fadeOut(node), SHOW_MS);
}

function fadeOut(node: HTMLElement): void {
  node.style.opacity = "0";
  setTimeout(() => {
    if (el === node) {
      node.remove();
      el = null;
    }
  }, FADE_MS);
}
