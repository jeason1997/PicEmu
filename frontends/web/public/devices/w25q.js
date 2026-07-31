import { labeledPart, picWiring, pin } from "./device.js";

const offsets = new Map();
let refreshPending = false;
let refreshGeneration = 0;
let lastRefresh = 0;

function wiring(context, part) {
  return picWiring(context, part, {
    "/CS": "csPin", CLK: "clockPin", DI: "mosiPin", DO: "misoPin"
  });
}

async function configure(context, part, strict = true) {
  const connection = wiring(context, part);
  if (!connection) {
    if (strict) throw new Error(`${part.id}需要把/CS、CLK、DI、DO连接到同一颗PIC`);
    return false;
  }
  part.attrs ||= {};
  context.setState(await context.post("/api/command", {
    command: "w25_config", mcuId: connection.mcuId,
    capacity: Number(part.attrs.capacity || 2097152),
    initialData: part.attrs.data || "", ...connection
  }));
  return true;
}

function formatDump(context, offset, data) {
  const rows = [];
  for (let index = 0; index < data.length; index += 16) {
    const bytes = data.slice(index, index + 16);
    const hexBytes = bytes.map(value =>
      value.toString(16).toUpperCase().padStart(2, "0")).join(" ");
    const ascii = bytes.map(value =>
      value >= 32 && value <= 126 ? String.fromCharCode(value) : ".").join("");
    rows.push(`${context.hex(offset + index, 6)}  ${hexBytes.padEnd(47)}  ${ascii}`);
  }
  return rows.join("\n");
}

async function refreshMemory(context, part) {
  if (!part || refreshPending) return;
  const connection = wiring(context, part);
  const dump = context.$("#w25Dump");
  if (!connection || !dump) return;
  const capacity = Number(part.attrs?.capacity || 2097152);
  let offset = Number(offsets.get(part.id) || 0);
  offset = Math.floor(Math.max(0, Math.min(capacity - 1, offset)) / 16) * 16;
  const generation = ++refreshGeneration;
  refreshPending = true;
  try {
    const result = await context.post("/api/command", {
      command: "w25_read", mcuId: connection.mcuId, offset,
      count: Math.min(256, capacity - offset)
    });
    if (generation !== refreshGeneration ||
        context.model.selected !== part.id || context.$("#w25Dump") !== dump) return;
    offsets.set(part.id, offset);
    const formatted = formatDump(context, offset, result.data);
    const selection = window.getSelection();
    const selectingDump = selection && !selection.isCollapsed &&
      ((selection.anchorNode && dump.contains(selection.anchorNode)) ||
       (selection.focusNode && dump.contains(selection.focusNode)));
    if (dump.textContent !== formatted && !selectingDump) dump.textContent = formatted;
    const offsetInput = context.$("#w25Offset");
    if (offsetInput) offsetInput.value = String(offset);
    const pageText = context.$("#w25Page");
    if (pageText) {
      pageText.textContent = `第 ${Math.floor(offset / 256) + 1} / ${
        Math.ceil(capacity / 256)} 页`;
    }
  } catch (error) {
    if (generation === refreshGeneration &&
        context.model.selected === part.id && context.$("#w25Dump") === dump) {
      dump.textContent = `读取失败：${error.message}`;
    }
  } finally {
    if (generation === refreshGeneration) refreshPending = false;
  }
}

export const capacities = [
  [131072, "W25Q10 · 128 KiB"],
  [262144, "W25Q20 · 256 KiB"],
  [524288, "W25Q40 · 512 KiB"],
  [1048576, "W25Q80 · 1 MiB"],
  [2097152, "W25Q16 · 2 MiB"],
  [4194304, "W25Q32 · 4 MiB"],
  [8388608, "W25Q64 · 8 MiB"],
  [16777216, "W25Q128 · 16 MiB"]
];

export default {
  type: "w25q",
  idPrefix: "flash",
  defaultAttrs: { capacity: 2097152, data: "" },
  category: "存储器",
  categoryOrder: 3,
  palette: {
    title: "W25Q Flash", detail: "SPI NOR",
    iconClass: "flash-icon", iconText: "▦", order: 1
  },
  className: "flash-chip",
  size: { width: 180, height: 150 },
  positionMode: "top-left",
  styles: `
    .flash-icon {
      border-radius:4px;border:2px solid #94a3b8;font-size:19px;
    }
    .flash-chip .part-body {
      width:180px;height:150px;background:#252d38;
      border:1px solid #94a3b8;border-radius:4px;
    }
    .flash-title {
      text-align:center;font:700 22px ui-monospace,monospace;
      padding-top:10px;letter-spacing:1px;
    }
    .flash-capacity-label {
      position:absolute;right:10px;bottom:8px;color:#8fa1b5;
      font:10px ui-monospace,monospace;
    }
    .w25-page-row,.w25-jump-row {
      display:grid;align-items:center;gap:6px;
    }
    .w25-page-row { grid-template-columns:1fr auto 1fr; }
    .w25-jump-row { grid-template-columns:1fr auto;margin-top:6px; }
    .w25-page-row span {
      min-width:92px;color:var(--muted);font-size:12px;text-align:center;
    }
    .property-row .w25-page-row button,
    .property-row .w25-jump-row button { margin-top:0; }
    .w25-jump-row input { width:100%; }
    .w25-dump {
      margin-top:8px;max-height:310px;overflow:auto;padding:8px;
      background:#0d131b;border:1px solid var(--line);border-radius:5px;
      color:#c8d4e3;font:11px/1.55 ui-monospace,monospace;white-space:pre;
      /* 明确允许选择，避免画布器件的 user-select:none 规则或浏览器默认行为干扰复制。 */
      user-select:text;cursor:text;
    }`,
  pins: [
    pin("/CS", "1 /CS", "left", 12),
    pin("DO", "2 DO (MISO)", "left", 42),
    pin("CLK", "6 CLK", "left", 72),
    pin("DI", "5 DI (MOSI)", "left", 102)
  ],
  render(part) {
    const capacity = Number(part.attrs?.capacity || 2097152);
    const capacityName = capacities.find(item => item[0] === capacity);
    const label = capacityName ? capacityName[1].split(" · ")[0] : `${capacity} B`;
    return labeledPart(`<div class="part-body">
      <i class="pin-one"></i><div class="flash-title">W25Q</div>
      <div class="flash-capacity-label">${label}</div></div>`, part.id);
  },
  configure,
  inspectorHtml(part) {
    return `<div class="property-row"><label>容量</label>
      <select id="propW25Capacity">${capacities.map(([value, label]) =>
        `<option value="${value}">${label}</option>`).join("")}</select></div>
      <div class="property-row"><label>写入地址0的数据（HEX）</label>
        <textarea id="propW25InitialData" name="w25-initial-${part.id}"
          rows="3" autocomplete="new-password" spellcheck="false"
          placeholder="例如：48 65 6C 6C 6F">${part.attrs?.data || ""}</textarea>
        <button id="w25Write" type="button">写入</button></div>
      <div class="property-row"><label>数据查看器</label>
        <div class="w25-page-row"><button id="w25Prev" type="button">上一页</button>
          <span id="w25Page">第 1 页</span>
          <button id="w25Next" type="button">下一页</button></div>
        <div class="w25-jump-row"><input id="w25Offset" type="number"
          min="0" step="256" value="0"><button id="w25Go" type="button">跳转</button></div>
        <pre id="w25Dump" class="w25-dump">等待读取……</pre></div>`;
  },
  bindInspector(context, part) {
    const attrs = part.attrs || (part.attrs = {});
    const capacitySelect = context.$("#propW25Capacity");
    capacitySelect.value = String(attrs.capacity || 2097152);
    capacitySelect.addEventListener("change", async event => {
      attrs.capacity = Number(event.target.value);
      try {
        await configure(context, part, true);
        offsets.set(part.id, 0);
        context.recordHistory();
        context.renderAll();
        context.showPartInspector(part);
        context.message(`${part.id}容量已更新，存储内容已按初始数据重新装载`);
      } catch (error) { context.message(error.message, true); }
    });
    const movePage = async delta => {
      const capacity = Number(attrs.capacity || 2097152);
      const current = Number(offsets.get(part.id) || 0);
      offsets.set(part.id, Math.max(0, Math.min(capacity - 1, current + delta)));
      await refreshMemory(context, part);
    };
    context.$("#w25Prev").addEventListener("click", () => movePage(-256));
    context.$("#w25Next").addEventListener("click", () => movePage(256));
    context.$("#w25Go").addEventListener("click", async () => {
      offsets.set(part.id, Number(context.$("#w25Offset").value || 0));
      await refreshMemory(context, part);
    });
    context.$("#w25Write").addEventListener("click", async () => {
      const connection = wiring(context, part);
      if (!connection) return context.message(`${part.id}的SPI连线不完整`, true);
      try {
        await context.post("/api/command", {
          command: "w25_write", mcuId: connection.mcuId, offset: 0,
          data: context.$("#propW25InitialData").value.trim()
        });
        offsets.set(part.id, 0);
        await refreshMemory(context, part);
        context.message(`${part.id}已从地址0写入输入数据`);
      } catch (error) { context.message(error.message, true); }
    });
    refreshMemory(context, part);
  },
  rename(context, part, oldId) {
    const offset = offsets.get(oldId);
    offsets.delete(oldId);
    if (offset !== undefined) offsets.set(part.id, offset);
  },
  remove(context, part) { offsets.delete(part.id); },
  diagramUnload() {
    refreshGeneration++;
    refreshPending = false;
    offsets.clear();
  },
  frame(context, part, now) {
    if (context.model.selected !== part.id || now - lastRefresh < 200) return;
    lastRefresh = now;
    refreshMemory(context, part);
  },
  loadSummary(parts) {
    const count = parts.length;
    return count ? `和 ${count} 个W25Q` : "";
  }
};
