const crypto = require("crypto");
const express = require("express");

const app = express();
const port = Number(process.env.PORT || 3187);

const size = Number(process.env.TEST_FILE_SIZE || 1073741824);
const fileName = process.env.TEST_FILE_NAME || "libdownloader-test.bin";
const etag = `"mock-etag-1gb"`;

// ═══════════════════════════════════════════════════════════════════════
// 启动时预计算全文件 MD5（标准答案）
// 数据模式: byte[i] = i % 251
// ═══════════════════════════════════════════════════════════════════════
const precomputeMd5 = (fileSize) => {
  const md5 = crypto.createHash("md5");
  const chunkSize = 1024 * 1024; // 1 MiB
  let offset = 0;

  console.log(`Pre-computing full-file MD5 for ${fileSize} bytes...`);
  const startTime = Date.now();

  while (offset < fileSize) {
    const remaining = fileSize - offset;
    const currentChunkSize = Math.min(chunkSize, remaining);

    const buffer = Buffer.allocUnsafe(currentChunkSize);
    for (let i = 0; i < currentChunkSize; i++) {
      buffer[i] = (offset + i) % 251;
    }

    md5.update(buffer);
    offset += currentChunkSize;
  }

  const hash = md5.digest("hex");
  const elapsed = Date.now() - startTime;
  return { hash, elapsed };
};

const fullFileMd5 = precomputeMd5(size);

console.log("");
console.log("══════════════════════════════════════════════════════");
console.log(`  Full-file MD5 (ground truth): ${fullFileMd5.hash}`);
console.log(`  File size                 : ${size} bytes (${(size / 1073741824).toFixed(2)} GiB)`);
console.log(`  Data pattern              : byte[i] = i % 251`);
console.log(`  Pre-compute time          : ${fullFileMd5.elapsed} ms`);
console.log("══════════════════════════════════════════════════════");
console.log("");

// ═══════════════════════════════════════════════════════════════════════
// 预计算每个可能 range 的 MD5（按实际 chunkSize 划分）
// 这样 range 请求完成时可以直接比对，不需要在线计算。
// ═══════════════════════════════════════════════════════════════════════
const kMinChunkSize = 1024 * 1024;
const kMaxChunkSize = 8 * 1024 * 1024;
const kChunkFanout = 8;

const precomputeRangeMd5s = (fileSize, threads = 8) => {
  const targetChunks = Math.max(threads * kChunkFanout, 1);
  const chunkSize = Math.min(Math.max(Math.floor(fileSize / targetChunks), kMinChunkSize), kMaxChunkSize);

  const rangeMd5s = new Map(); // key: "start-end" → md5 hex
  let offset = 0;

  while (offset < fileSize) {
    const start = offset;
    const end = Math.min(start + chunkSize - 1, fileSize - 1);
    const rangeSize = end - start + 1;

    const md5 = crypto.createHash("md5");
    let pos = start;
    while (pos <= end) {
      const remaining = end - pos + 1;
      const bufSize = Math.min(1024 * 1024, remaining);
      const buffer = Buffer.allocUnsafe(bufSize);
      for (let i = 0; i < bufSize; i++) {
        buffer[i] = (pos + i) % 251;
      }
      md5.update(buffer);
      pos += bufSize;
    }

    rangeMd5s.set(`${start}-${end}`, {
      md5: md5.digest("hex"),
      start,
      end,
      size: rangeSize,
    });

    offset += chunkSize;
  }

  return { rangeMd5s, chunkSize, rangeCount: rangeMd5s.size };
};

const threads = Number(process.env.TEST_THREADS || 8);
const { rangeMd5s, chunkSize: preChunkSize, rangeCount } = precomputeRangeMd5s(size, threads);

console.log(`Pre-computed ${rangeCount} range MD5s (chunkSize=${preChunkSize})`);
console.log("");

// ── /health ──────────────────────────────────────────────────────────
app.get("/health", (req, res) => {
  res.json({ ok: true, size, fileName });
});

// ── /checksum ────────────────────────────────────────────────────────
// 返回预计算的全文件 MD5 和各 range MD5，方便客户端验证。
app.get("/checksum", (req, res) => {
  const ranges = {};
  for (const [key, val] of rangeMd5s) {
    ranges[key] = val.md5;
  }
  res.json({
    fullFileMd5: fullFileMd5.hash,
    fileSize: size,
    dataPattern: "byte[i] = i % 251",
    rangeChunkSize: preChunkSize,
    rangeCount,
    rangeMd5s: ranges,
  });
});

// ── 共享的 Range 解析与发送逻辑 ──────────────────────────────────────
function handleFileRequest(req, res, requireRange) {
  console.log(`\n--- Request: ${req.method} ${req.url} ---`);
  console.log("Request Headers:", JSON.stringify(req.headers, null, 2));

  res.setHeader("Accept-Ranges", "bytes");
  res.setHeader("Content-Type", "application/octet-stream");
  res.setHeader("Content-Disposition", `attachment; filename="${fileName}"`);
  res.setHeader("ETag", etag);

  const range = req.headers.range;

  const sendHeaders = (statusCode) => {
    res.status(statusCode);
    console.log(`Response Status: ${statusCode}`);
  };

  // 无 Range 头
  if (!range) {
    if (requireRange) {
      sendHeaders(400);
      res.json({ error: "Range header is required for this endpoint" });
      return;
    }
    // 全文件下载 (200)
    res.setHeader("Content-Length", String(size));
    sendHeaders(200);
    sendVirtualFile(req, res, 0, size - 1);
    return;
  }

  // 解析 Range
  const match = /^bytes=(\d*)-(\d*)$/.exec(range);
  if (!match) {
    res.setHeader("Content-Range", `bytes */${size}`);
    sendHeaders(416);
    res.end();
    return;
  }

  const start = match[1] === "" ? 0 : Number(match[1]);
  const end = match[2] === "" ? size - 1 : Number(match[2]);

  if (
    !Number.isInteger(start) ||
    !Number.isInteger(end) ||
    start < 0 ||
    end < start ||
    end >= size
  ) {
    res.setHeader("Content-Range", `bytes */${size}`);
    sendHeaders(416);
    res.end();
    return;
  }

  // 合法分片 → 206
  const contentLength = end - start + 1;
  res.setHeader("Content-Range", `bytes ${start}-${end}/${size}`);
  res.setHeader("Content-Length", String(contentLength));

  sendHeaders(206);
  sendVirtualFile(req, res, start, end);
}

// ── /file ─────────────────────────────────────────────────────────────
// 通用端点：兼容无 Range（200）和有 Range（206）。
app.get("/file", (req, res) => handleFileRequest(req, res, false));

// ── /file-range ───────────────────────────────────────────────────────
// 多线程下载专用：强制要求 Range 头。
app.get("/file-range", (req, res) => handleFileRequest(req, res, true));

// ── sendVirtualFile ───────────────────────────────────────────────────
// 传输虚拟文件数据。完成后将在线计算的 MD5 与预计算值比对。
function sendVirtualFile(req, res, start, end) {
  let current = start;
  const chunkSize = 64 * 1024;
  let transferDone = false;
  const md5 = crypto.createHash("md5");

  req.on("close", () => {
    if (transferDone) return;
    console.log(
      `[Abort] Connection closed by client. Progress: ${current}/${end}`,
    );
  });

  function finish() {
    if (transferDone) return;
    transferDone = true;
    const onlineHash = md5.digest("hex");
    const rangeKey = `${start}-${end}`;
    const precomputed = rangeMd5s.get(rangeKey);
    const match = precomputed ? (onlineHash === precomputed.md5 ? "✓ MATCH" : "✗ MISMATCH") : "? (no precomputed)";

    const rangeLabel =
      start === 0 && end === size - 1
        ? `FULL (${size} bytes)`
        : `${start}-${end}`;

    console.log(
      `[Done] Transfer complete: ${rangeLabel}\n` +
      `       Online MD5 : ${onlineHash}\n` +
      `       Expected   : ${precomputed ? precomputed.md5 : "N/A"}\n` +
      `       Verdict    : ${match}`
    );

    if (precomputed && onlineHash !== precomputed.md5) {
      console.error(
        `[ERROR] MD5 mismatch for range ${rangeKey}! This indicates a bug in sendVirtualFile.`
      );
    }

    res.end();
  }

  function writeNext() {
    if (transferDone) return;

    let canWrite = true;
    while (current <= end && canWrite && !transferDone) {
      const remaining = end - current + 1;
      const currentChunkSize = Math.min(chunkSize, remaining);

      const buffer = Buffer.allocUnsafe(currentChunkSize);
      for (let i = 0; i < currentChunkSize; i++) {
        buffer[i] = (current + i) % 251;
      }

      md5.update(buffer);
      current += currentChunkSize;
      canWrite = res.write(buffer);
    }

    if (current > end && !transferDone) {
      finish();
    }
  }

  res.on("drain", writeNext);
  writeNext();
}

// ═══════════════════════════════════════════════════════════════════════
app.listen(port, "127.0.0.1", () => {
  console.log(`Test server listening on http://127.0.0.1:${port}`);
  console.log(`  Full-file MD5: ${fullFileMd5.hash}`);
  console.log(`  /file          – generic (Range optional)`);
  console.log(`  /file-range    – multi-thread (Range required)`);
  console.log(`  /checksum      – pre-computed MD5 reference`);
  console.log(`  /health        – health check`);
});
