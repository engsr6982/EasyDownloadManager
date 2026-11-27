import { api } from "@platform";

/** 把带 data-i18n 标记的节点文本替换为词条（词条缺失时适配层原样返回 key） */
export function applyI18n(root: ParentNode): void {
  for (const el of Array.from(root.querySelectorAll<HTMLElement>("[data-i18n]"))) {
    const key = el.dataset.i18n;
    if (key) el.textContent = api.i18n(key);
  }
}
