const express = require("express");

const app = express();
const port = Number(process.env.PORT || 3187);

// 核心修改：升级为 1GB (1024 * 1024 * 1024)
const size = Number(process.env.TEST_FILE_SIZE || 1073741824);
const fileName = process.env.TEST_FILE_NAME || "libdownloader-test.bin";

app.get("/health", (req, res) => {
  res.json({ ok: true, size, fileName });
});

app.get("/file", (req, res) => {
  console.log(`[>>] request: ${req.url}`);
  res.setHeader("Accept-Ranges", "bytes");
  res.setHeader("Content-Type", "application/octet-stream");
  res.setHeader("Content-Disposition", `attachment; filename="${fileName}"`);

  const range = req.headers.range;

  // 1. 处理没有 Range 的普通下载 (完整 1GB 下载)
  if (!range) {
    res.setHeader("Content-Length", size);
    res.status(200);
    sendVirtualFile(res, 0, size - 1);
    console.log(`[<<] simple http download request, return 200...`);
    return;
  }

  // 2. 解析 Range 请求
  const match = /^bytes=(\d*)-(\d*)$/.exec(range);
  if (!match) {
    res.status(416).end();
    console.log(`[<<] invalid range request, return 416...`);
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
    res.status(416).end();
    console.log(`[<<] invalid range request, return 416...`);
    return;
  }

  console.log(`[>>] request range: ${start}-${end}`);

  // 3. 返回 206 局部内容
  const contentLength = end - start + 1;
  res.status(206);
  res.setHeader("Content-Range", `bytes ${start}-${end}/${size}`);
  res.setHeader("Content-Length", contentLength);

  sendVirtualFile(res, start, end);
});

// 虚拟文件生成器：不占内存，按需通过流输出特定区间的虚拟数据
function sendVirtualFile(res, start, end) {
  let current = start;
  const chunkSize = 64 * 1024; // 每次发送 64KB 的数据块

  function writeNext() {
    let canWrite = true;
    while (current <= end && canWrite) {
      const remaining = end - current + 1;
      const currentChunkSize = Math.min(chunkSize, remaining);

      const buffer = Buffer.allocUnsafe(currentChunkSize);
      for (let i = 0; i < currentChunkSize; i++) {
        // 保持算法与原先一致：i + current 即当前的绝对 offset
        buffer[i] = (current + i) % 251;
      }

      current += currentChunkSize;
      canWrite = res.write(buffer);
    }

    if (current > end) {
      res.end();
    }
  }

  res.on("drain", writeNext);
  writeNext();
}

app.listen(port, "127.0.0.1", () => {
  console.log(
    `libdownloader test server listening on http://127.0.0.1:${port}`,
  );
  console.log(`Virtual file size target: ${size} bytes (~1GB)`);
  console.log(`Test Download url: http://127.0.0.1:${port}/file`);
});
