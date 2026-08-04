import { labeledPart, pin } from "./device.js";

const dimension = (value, fallback) =>
  Math.max(1, Math.min(32, Number.parseInt(value, 10) || fallback));

function matrixConfig(part) {
  return {
    rows: dimension(part?.attrs?.rows, 2),
    cols: dimension(part?.attrs?.cols, 2),
    wiring: String(part?.attrs?.wiring || "Z").toUpperCase() === "S" ? "S" : "Z"
  };
}

function wiring(context, part) {
  const connection = context.connectedMcuPin(part, "DIN");
  const { rows, cols } = matrixConfig(part);
  return connection ? {
    mcuId: connection.mcuId,
    dataPin: connection.gpio, count: rows * cols
  } : null;
}

async function configure(context, part, strict = true) {
  const connection = wiring(context, part);
  if (!connection) {
    if (strict) throw new Error(`${part.id} 的 DIN 必须连接到 MCU GPIO`);
    return false;
  }
  context.setState(await context.post("/api/command", {
    command: "ws2812_config", ...connection
  }));
  return true;
}

export default {
  type: "ws2812", idPrefix: "matrix",
  defaultAttrs: { rows: 2, cols: 2, wiring: "Z" },
  category: "输出器件", categoryOrder: 1,
  palette: {
    title: "WS2812 矩阵", detail: "可配置 RGB 点阵",
    iconClass: "ws2812-icon", order: 4
  },
  className: "ws2812-part", positionMode: "top-left",
  size(part) {
    const { rows, cols } = matrixConfig(part);
    return { width: cols * 40 + 20, height: rows * 40 + 20 };
  },
  pins: [pin("DIN", "DIN", "left", 8)],
  styles: `
    .ws2812-icon{width:26px;height:26px;border-radius:6px;
      background:conic-gradient(#f44,#4f4,#48f,#f44)}
    .ws2812-part .part-body{padding:12px;display:grid;gap:8px;
      background:#242a31;border:2px solid #58616d;border-radius:8px;
      box-sizing:border-box}
    .ws2812-pixel{width:32px;height:32px;border-radius:6px;
      background:#181b20;box-shadow:inset 0 0 6px #fff2}`,
  render(part) {
    const { rows, cols } = matrixConfig(part);
    const pixels = Array.from({ length: rows * cols }, (_, index) =>
      `<i class="ws2812-pixel" data-led="${index}"></i>`).join("");
    return labeledPart(
      `<div class="part-body" style="grid-template-columns:repeat(${cols},32px)">${pixels}</div>`,
      part.id
    );
  },
  configure,
  update(context, part, element) {
    const connection = context.connectedMcuPin(part, "DIN");
    const colors = connection?.state?.ws2812?.colors || [];
    const { rows, cols, wiring } = matrixConfig(part);
    element.querySelectorAll(".ws2812-pixel").forEach((pixel, logical) => {
      const row = Math.floor(logical / cols);
      const col = logical % cols;
      const physical = row * cols +
        (wiring === "S" && row % 2 === 1 ? cols - 1 - col : col);
      const [r = 0, g = 0, b = 0] = colors[physical] || [];
      pixel.style.background = `rgb(${r},${g},${b})`;
      pixel.style.boxShadow = r || g || b
        ? `0 0 18px rgba(${r},${g},${b},.85)` : "inset 0 0 6px #fff2";
    });
  },
  inspectorHtml() {
    return `<div class="property-row"><label>行数</label><input id="propRows" type="number" min="1" max="32"></div>
      <div class="property-row"><label>列数</label><input id="propCols" type="number" min="1" max="32"></div>
      <div class="property-row"><label>走线</label><select id="propWiring">
        <option value="Z">Z 型（逐行同向）</option><option value="S">S 型（蛇形）</option>
      </select></div>`;
  },
  bindInspector(context, part) {
    const config = matrixConfig(part);
    const rows = context.$("#propRows");
    const cols = context.$("#propCols");
    const wiringMode = context.$("#propWiring");
    rows.value = config.rows; cols.value = config.cols; wiringMode.value = config.wiring;
    const update = () => {
      part.attrs ||= {};
      part.attrs.rows = dimension(rows.value, 2);
      part.attrs.cols = dimension(cols.value, 2);
      part.attrs.wiring = wiringMode.value;
      context.renderAll(); context.recordHistory();
      configure(context, part, false).catch(error => context.message(error.message, true));
    };
    rows.addEventListener("change", update);
    cols.addEventListener("change", update);
    wiringMode.addEventListener("change", update);
  }
};
