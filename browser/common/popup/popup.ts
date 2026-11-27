import { api } from "@platform";
import { DEFAULT_HOST, DEFAULT_PORT, STORAGE_KEYS } from "@common/data/constants";
import { applyI18n } from "@common/i18n";
import { loadSettings, type Settings } from "@common/settings";
import type { ConnectionTestResult } from "@common/types/messages";

const SWITCH_IDS = ["enabled", "autoCapture", "sniffPanel", "contextMenu"] as const;

const host = document.getElementById("host") as HTMLInputElement;
const port = document.getElementById("port") as HTMLInputElement;
const token = document.getElementById("token") as HTMLInputElement;
const status = document.getElementById("status") as HTMLParagraphElement;
const target = document.getElementById("target") as HTMLParagraphElement;
const featureRows = Array.from(document.querySelectorAll<HTMLElement>("[data-feature]"));

function switchEl(id: (typeof SWITCH_IDS)[number]): HTMLInputElement {
  return document.getElementById(id) as HTMLInputElement;
}

/** 读取表单并去除地址与令牌首尾空白。 */
function readForm(): Settings {
  return {
    enabled: switchEl("enabled").checked,
    autoCapture: switchEl("autoCapture").checked,
    sniffPanel: switchEl("sniffPanel").checked,
    contextMenu: switchEl("contextMenu").checked,
    host: host.value.trim(),
    port: Number.parseInt(port.value, 10),
    token: token.value.trim(),
  };
}

function validate(f: Settings): boolean {
  return !!f.host && Number.isInteger(f.port) && f.port > 0 && f.port <= 65535;
}

function setStatus(key: string, bad: boolean): void {
  status.textContent = api.i18n(key);
  status.className = bad ? "bad" : "ok";
}

function updateTarget(f: Settings): void {
  target.textContent = `ws://${f.host || DEFAULT_HOST}:${f.port || DEFAULT_PORT}`;
}

/** 总开关关闭时功能行不可操作；连接配置保持可用 */
function applyAvailability(): void {
  const on = switchEl("enabled").checked;
  for (const row of featureRows) row.classList.toggle("disabled", !on);
  for (const id of SWITCH_IDS.slice(1)) switchEl(id).disabled = !on;
}

/** 校验并保存设置，随后通知后台应用变更。 */
async function save(): Promise<void> {
  const f = readForm();
  if (!validate(f)) {
    setStatus("statusInvalid", true);
    return;
  }
  await api.storageSet({
    [STORAGE_KEYS.enabled]: f.enabled,
    [STORAGE_KEYS.autoCapture]: f.autoCapture,
    [STORAGE_KEYS.sniffPanel]: f.sniffPanel,
    [STORAGE_KEYS.contextMenu]: f.contextMenu,
    [STORAGE_KEYS.host]: f.host,
    [STORAGE_KEYS.port]: f.port,
    [STORAGE_KEYS.token]: f.token,
  });
  updateTarget(f);
  await api.request({ kind: "settings:changed" }).catch(() => undefined);
  setStatus("statusSaved", false);
}

/** 按当前表单参数测试连接并更新状态文本。 */
async function testConnection(): Promise<void> {
  const f = readForm();
  if (!validate(f)) {
    setStatus("statusInvalid", true);
    return;
  }
  setStatus("statusTesting", false);
  const res = (await api
    .request({ kind: "testConnection", host: f.host, port: f.port, token: f.token })
    .catch(() => undefined)) as ConnectionTestResult | undefined;
  if (!res || !res.reachable) setStatus("statusUnreachable", true);
  else if (!res.authenticated) setStatus("statusBadToken", true);
  else setStatus("statusOk", false);
}

/** 初始化词条、折叠区与设置表单，并绑定自动保存。 */
export function startPopup(): void {
  applyI18n(document);
  for (const head of document.querySelectorAll<HTMLButtonElement>(".expander > .head")) {
    const section = head.closest<HTMLElement>(".expander")!;
    const body = document.getElementById(head.getAttribute("aria-controls")!) as HTMLElement;
    head.addEventListener("click", () => {
      const open = section.classList.toggle("open");
      head.setAttribute("aria-expanded", String(open));
      body.inert = !open;
      body.setAttribute("aria-hidden", String(!open));
    });
  }
  void (async () => {
    const s = await loadSettings();
    for (const id of SWITCH_IDS) switchEl(id).checked = s[id];
    host.value = s.host;
    port.value = String(s.port);
    token.value = s.token;
    updateTarget(s);
    applyAvailability();
  })();

  for (const id of SWITCH_IDS) {
    switchEl(id).addEventListener("change", () => {
      applyAvailability();
      void save();
    });
  }
  document.getElementById("save")?.addEventListener("click", () => void save());
  document.getElementById("testConnection")?.addEventListener("click", () => void testConnection());
  host.addEventListener("input", () => updateTarget(readForm()));
  port.addEventListener("input", () => updateTarget(readForm()));
}
