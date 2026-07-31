import { labeledPart, picWiring, pin } from "./device.js";

export function wiring(context, part) {
  return picWiring(context, part, {
    DIN: "dataPin", CLK: "clockPin", LOAD: "loadPin"
  });
}

async function configure(context, part, strict = true) {
  const connection = wiring(context, part);
  if (!connection) {
    if (strict) throw new Error(`${part.id} 的 DIN、CLK、LOAD 必须连接到同一颗 PIC`);
    return false;
  }
  context.setState(await context.post("/api/command", {
    command: "max7219_config", mcuId: connection.mcuId, ...connection
  }));
  return true;
}

export default {
  type: "max7219", idPrefix: "driver",
  category: "显示驱动", categoryOrder: 2,
  palette: { title: "MAX7219", detail: "8×8 LED 恒流驱动", iconText: "7219", order: 0 },
  className: "max7219-part", size: { width: 190, height: 235 },
  positionMode: "top-left",
  styles: `
    .max7219-part .part-body { width:190px;height:235px;background:#202a35;
      border:3px solid #8594a5;border-radius:7px;box-shadow:inset 0 0 18px #0008; }
    .max7219-title { position:absolute;left:55px;top:103px;color:#e6edf5;
      font:700 16px ui-monospace,monospace; }`,
  pins: [
    pin("DIN", "DIN", "left", 25), pin("CLK", "CLK", "left", 75),
    pin("LOAD", "LOAD", "left", 125),
    ...Array.from({length: 8}, (_, i) => pin(`SEG${i}`, `S${i}`, "right", 8 + i * 13)),
    ...Array.from({length: 8}, (_, i) => pin(`DIG${i}`, `D${i}`, "right", 112 + i * 13))
  ],
  render(part) {
    return labeledPart(`<div class="part-body"><div class="max7219-title">MAX7219</div></div>`, part.id);
  },
  configure
};
