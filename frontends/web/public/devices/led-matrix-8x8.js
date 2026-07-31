import { labeledPart, pin } from "./device.js";
import { wiring as max7219Wiring } from "./max7219.js";

function wiring(context, part) {
  let chipId = null;
  for (const group of ["SEG", "DIG"]) {
    for (let index = 0; index < 8; index++) {
      const endpoint = `${part.id}:${group}${index}`;
      const connection = context.model.diagram.connections.find(item =>
        item[0] === endpoint || item[1] === endpoint);
      if (!connection) return null;
      const other = connection[0] === endpoint ? connection[1] : connection[0];
      const match = new RegExp(`^([^:]+):${group}${index}$`).exec(other);
      if (!match || (chipId !== null && chipId !== match[1])) return null;
      chipId = match[1];
    }
  }
  const chip = context.model.diagram.parts.find(item =>
    item.id === chipId && item.type === "max7219");
  const chipConnection = chip ? max7219Wiring(context, chip) : null;
  return chipConnection ? { chipId, mcuId: chipConnection.mcuId } : null;
}

function configure(context, part, strict = true) {
  if (wiring(context, part)) return true;
  if (strict) throw new Error(`${part.id} 必须按序连接 MAX7219 的 SEG0～7 和 DIG0～7`);
  return false;
}

export default {
  type: "led-matrix-8x8", idPrefix: "matrix",
  defaultAttrs: { color: "red", commonCathode: true },
  category: "输出器件", categoryOrder: 1,
  palette: { title: "8×8 LED 点阵", detail: "共阴极行扫描", iconText: "▦", order: 4 },
  className: "led-matrix-part", size: { width: 245, height: 245 },
  positionMode: "top-left",
  styles: `
    .led-matrix-part .part-body { width:245px;height:245px;background:#190d10;
      border:3px solid #71303a;border-radius:10px;box-shadow:inset 0 0 22px #000; }
    .led-matrix-grid { position:absolute;left:42px;top:26px;display:grid;
      grid-template-columns:repeat(8,17px);gap:5px; }
    .led-matrix-grid i { width:15px;height:15px;border-radius:50%;background:#4c171d; }
    .led-matrix-grid i.on { background:#ff3545;box-shadow:0 0 9px #ff3545; }`,
  pins: [
    ...Array.from({length: 8}, (_, i) => pin(`SEG${i}`, `S${i}`, "left", 8 + i * 27)),
    ...Array.from({length: 8}, (_, i) => pin(`DIG${i}`, `D${i}`, "right", 8 + i * 27))
  ],
  render(part) {
    return labeledPart(`<div class="part-body"><div class="led-matrix-grid">${
      Array.from({length: 64}, (_, i) => `<i data-led="${i}"></i>`).join("")
    }</div></div>`, part.id);
  },
  configure,
  update(context, part, element) {
    const connection = wiring(context, part);
    const state = connection
      ? context.model.states.get(connection.mcuId)?.max7219 : null;
    if (!state?.rows) return;
    for (let row = 0; row < 8; row++) for (let column = 0; column < 8; column++) {
      element.querySelector(`[data-led="${row * 8 + column}"]`)?.classList.toggle(
        "on", (state.rows[row] & (1 << column)) !== 0);
    }
  }
};
