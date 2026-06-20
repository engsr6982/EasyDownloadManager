const express = require("express");

const app = express();
const port = Number(process.env.PORT || 3187);

const size = Number(process.env.TEST_FILE_SIZE || 1073741824);
const fileName = process.env.TEST_FILE_NAME || "libdownloader-test.bin";
const etag = `"mock-etag-1gb"`;

app.get("/health", (req, res) => {
  res.json({ ok: true, size, fileName });
});

app.get("/file", (req, res) => {
  console.log(`\n--- Request: ${req.method} ${req.url} ---`);
  console.log("Request Headers:\n", req.headers);

  res.setHeader("Accept-Ranges", "bytes");
  res.setHeader("Content-Type", "application/octet-stream");
  res.setHeader("Content-Disposition", `attachment; filename="${fileName}"`);
  res.setHeader("ETag", etag);

  const range = req.headers.range;

  const sendHeaders = (statusCode) => {
    res.status(statusCode);
    console.log(`Response Status: ${statusCode}`);
    console.log("Response Headers:\n", res.getHeaders());
  };

  // 1. 无 Range 请求 (200)
  if (!range) {
    res.setHeader("Content-Length", String(size));
    sendHeaders(200);
    sendVirtualFile(req, res, 0, size - 1);
    return;
  }

  // 2. 解析 Range 请求
  const match = /^bytes=(\d*)-(\d*)$/.exec(range);
  if (!match) {
    res.setHeader("Content-Range", `bytes */${size}`);
    sendHeaders(416);
    res.end();
    return;
  }

  const start = match[1] === "" ? 0 : Number(match[1]);
  const end = match[2] === "" ? size - 1 : Number(match[2]);

  // 3. 校验 Range 边界 (416)
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

  // 4. 合法分片请求 (206)
  const contentLength = end - start + 1;
  res.setHeader("Content-Range", `bytes ${start}-${end}/${size}`);
  res.setHeader("Content-Length", String(contentLength));

  sendHeaders(206);
  sendVirtualFile(req, res, start, end);
});

function sendVirtualFile(req, res, start, end) {
  let current = start;
  const chunkSize = 64 * 1024;
  let isClosed = false;

  req.on("close", () => {
    isClosed = true;
    console.log(
      `[Abort] Connection closed by client. Progress: ${current}/${end}`,
    );
  });

  function writeNext() {
    if (isClosed) return;

    let canWrite = true;
    while (current <= end && canWrite && !isClosed) {
      const remaining = end - current + 1;
      const currentChunkSize = Math.min(chunkSize, remaining);

      const buffer = Buffer.allocUnsafe(currentChunkSize);
      for (let i = 0; i < currentChunkSize; i++) {
        buffer[i] = (current + i) % 251;
      }

      current += currentChunkSize;
      canWrite = res.write(buffer);
    }

    if (current > end && !isClosed) {
      console.log(`[Done] Transfer complete: ${start}-${end}`);
      res.end();
    }
  }

  res.on("drain", writeNext);
  writeNext();
}

app.listen(port, "127.0.0.1", () => {
  console.log(`Test server running on http://127.0.0.1:${port}/file`);
});
