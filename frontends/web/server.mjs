import { createServer } from "node:http";
import { spawn } from "node:child_process";
import { promises as fs } from "node:fs";
import path from "node:path";
import process from "node:process";
import readline from "node:readline";

const repoRoot = path.resolve(process.cwd());
const webRoot = path.join(repoRoot, "frontends", "web", "public");
const buildRoot = path.join(repoRoot, "build", "web");
const commandLine = process.argv.slice(2);
function option(name, fallback) {
  const index = commandLine.indexOf(name);
  return index >= 0 && commandLine[index + 1] !== undefined
    ? commandLine[index + 1] : fallback;
}
const port = Number(option("--port", process.env.PICEMU_WEB_PORT || 4173));
const initialExample = option(
  "--example", process.env.PICEMU_WEB_EXAMPLE || "button");

await fs.mkdir(path.join(buildRoot, "uploads"), { recursive: true });

const backend = process.platform === "win32"
  ? spawn("wsl.exe", [
      "--cd", repoRoot, "bash", "-lc", "./build/picemu-web-core"
    ], { stdio: ["pipe", "pipe", "inherit"] })
  : spawn(path.join(repoRoot, "build", "picemu-web-core"), [], {
      stdio: ["pipe", "pipe", "inherit"]
    });

const lines = readline.createInterface({ input: backend.stdout });
const pending = [];
lines.on("line", line => {
  const request = pending.shift();
  if (!request) return;
  try {
    request.resolve(JSON.parse(line));
  } catch (error) {
    request.reject(new Error(`后端返回了无效JSON：${line}`));
  }
});
backend.on("exit", code => {
  while (pending.length) {
    pending.shift().reject(new Error(`仿真后端已退出：${code}`));
  }
});

function backendCommand(fields) {
  return new Promise((resolve, reject) => {
    pending.push({ resolve, reject });
    backend.stdin.write(fields.join("\t") + "\n");
  });
}

function json(response, status, value) {
  response.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store"
  });
  response.end(JSON.stringify(value));
}

async function readBody(request, limit = 2 * 1024 * 1024) {
  const chunks = [];
  let size = 0;
  for await (const chunk of request) {
    size += chunk.length;
    if (size > limit) throw new Error("请求内容过大");
    chunks.push(chunk);
  }
  return Buffer.concat(chunks);
}

function safeRepoPath(relative) {
  const resolved = path.resolve(repoRoot, relative);
  if (resolved !== repoRoot &&
      !resolved.startsWith(repoRoot + path.sep)) {
    throw new Error("路径超出项目目录");
  }
  return resolved;
}

/*
 * C 后端在 Windows 上运行于 WSL，不能直接识别 E:\... 形式的路径。
 * Node 仍使用 Windows 路径做越界检查和文件操作，只在发给后端时转换。
 */
function backendPath(file) {
  if (process.platform !== "win32") return file;
  const match = /^([A-Za-z]):[\\/](.*)$/.exec(file);
  if (!match) throw new Error(`无法转换为 WSL 路径：${file}`);
  return `/mnt/${match[1].toLowerCase()}/${match[2].replaceAll("\\", "/")}`;
}

async function listExamples() {
  const root = path.join(repoRoot, "examples");
  const entries = await fs.readdir(root, { withFileTypes: true });
  const result = [];
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const diagram = path.join(root, entry.name, "diagram.json");
    try {
      await fs.access(diagram);
      result.push({
        name: entry.name,
        diagram: `examples/${entry.name}/diagram.json`
      });
    } catch {
      // 只有包含diagram.json的示例才出现在网页列表中。
    }
  }
  return result;
}

async function api(request, response, url) {
  if (request.method === "GET" && url.pathname === "/api/examples") {
    return json(response, 200, {
      ok: true,
      initialExample,
      examples: await listExamples()
    });
  }
  if (request.method === "GET" && url.pathname === "/api/file") {
    const file = safeRepoPath(url.searchParams.get("path") || "");
    const data = await fs.readFile(file);
    response.writeHead(200, {
      "Content-Type": file.endsWith(".json")
        ? "application/json; charset=utf-8"
        : "text/plain; charset=utf-8",
      "Cache-Control": "no-store"
    });
    return response.end(data);
  }

  if (request.method !== "POST") return false;
  const body = await readBody(request);
  const value = body.length ? JSON.parse(body.toString("utf8")) : {};

  if (url.pathname === "/api/load") {
    const firmware = safeRepoPath(value.firmware);
    const result = await backendCommand([
      "load", backendPath(firmware), value.device || "PIC10F200"
    ]);
    result.firmware = "build/web/uploads/firmware.hex";
    return json(response, result.ok ? 200 : 400, result);
  }
  if (url.pathname === "/api/upload-firmware") {
    const firmware = path.join(buildRoot, "uploads", "firmware.hex");
    await fs.writeFile(firmware, value.text || "", "utf8");
    const result = await backendCommand([
      "load", backendPath(firmware), value.device || "PIC10F200"
    ]);
    return json(response, result.ok ? 200 : 400, result);
  }
  if (url.pathname === "/api/command") {
    const command = value.command;
    let fields;
    if (command === "run") {
      fields = ["run", value.cycles || 0,
                value.inputMask || 0, value.inputValues || 0];
    } else if (command === "step") {
      fields = ["step", value.inputMask || 0, value.inputValues || 0];
    } else if (["reset", "state", "flash"].includes(command)) {
      fields = [command];
    } else {
      return json(response, 400, { ok: false, error: "未知命令" });
    }
    const result = await backendCommand(fields);
    return json(response, result.ok ? 200 : 400, result);
  }
  return false;
}

const mime = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".css", "text/css; charset=utf-8"],
  [".svg", "image/svg+xml"]
]);

const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url, `http://${request.headers.host}`);
    if (url.pathname.startsWith("/api/")) {
      const handled = await api(request, response, url);
      if (handled !== false) return;
      return json(response, 404, { ok: false, error: "API不存在" });
    }

    const relative = url.pathname === "/" ? "index.html"
      : decodeURIComponent(url.pathname.slice(1));
    const file = path.resolve(webRoot, relative);
    if (!file.startsWith(webRoot + path.sep) && file !== webRoot) {
      response.writeHead(403);
      return response.end("Forbidden");
    }
    const data = await fs.readFile(file);
    response.writeHead(200, {
      "Content-Type": mime.get(path.extname(file)) ||
        "application/octet-stream"
    });
    response.end(data);
  } catch (error) {
    if (!response.headersSent) {
      json(response, 500, { ok: false, error: error.message });
    } else {
      response.end();
    }
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`PicEmu Web: http://127.0.0.1:${port}`);
});

function shutdown() {
  backend.kill();
  server.close(() => process.exit(0));
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
