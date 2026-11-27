// 双目标编译期切换构建：chromium 与 firefox 各产出完整扩展目录到 dist/<target>。
// 平台差异只在 @platform 别名解析处发生，另一套适配器不进入产物。
import { build } from "vite";
import { cpSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const targets = process.argv.slice(2).filter((t) => t === "chromium" || t === "firefox");
const buildTargets = targets.length ? targets : ["chromium", "firefox"];

// 每个入口独立打成 iife：MV3 内容脚本不支持 ES module，后台/弹窗统一 iife 最稳妥。
const entries = { background: "background.js", content: "content.js", popup: "popup.js" };

/** assets/i18n 的扁平词条转成浏览器要求的 _locales/<localeCode>/messages.json */
const LOCALE_DIRS = { en_US: "en_US", zh_CN: "zh_CN" };

function writeLocales(outDir) {
  const src = resolve(here, "assets", "i18n");
  for (const file of readdirSync(src)) {
    const name = file.replace(/\.json$/, "");
    const locale = LOCALE_DIRS[name];
    if (!locale) continue;
    const flat = JSON.parse(readFileSync(join(src, file), "utf8"));
    const messages = Object.fromEntries(Object.entries(flat).map(([key, value]) => [key, { message: value }]));
    const dir = join(outDir, "_locales", locale);
    mkdirSync(dir, { recursive: true });
    writeFileSync(join(dir, "messages.json"), `${JSON.stringify(messages, null, 2)}\n`);
  }
}

async function buildTarget(target) {
  const outDir = resolve(here, "dist", target);
  rmSync(outDir, { recursive: true, force: true });
  mkdirSync(outDir, { recursive: true });

  for (const [name, outFile] of Object.entries(entries)) {
    await build({
      root: here,
      logLevel: "warn",
      configFile: false,
      resolve: {
        alias: {
          "@platform": resolve(here, target, "adapter.ts"),
          "@common": resolve(here, "common"),
        },
      },
      build: {
        outDir,
        emptyOutDir: false,
        minify: "esbuild",
        target: "es2022",
        lib: {
          entry: resolve(here, "common", "entry", `${name}.ts`),
          name: `edm${name}`,
          formats: ["iife"],
        },
        rollupOptions: {
          output: { entryFileNames: outFile, chunkFileNames: "[name].js", assetFileNames: "[name][extname]" },
        },
      },
    });
  }

  cpSync(resolve(here, target, "manifest.json"), join(outDir, "manifest.json"));
  cpSync(join(resolve(here, "common", "popup"), "popup.html"), join(outDir, "popup.html"));
  mkdirSync(join(outDir, "assets"), { recursive: true });
  cpSync(resolve(here, "assets", "icons"), join(outDir, "assets", "icons"), { recursive: true });
  writeLocales(outDir);

  for (const needed of ["background.js", "content.js", "popup.js", "popup.html", "manifest.json", "assets/icons/icon16.png"]) {
    if (!existsSync(join(outDir, needed))) throw new Error(`${target} 产物缺 ${needed}`);
  }
  console.log(`built ${target} -> ${outDir}`);
}

for (const target of buildTargets) {
  await buildTarget(target);
}
