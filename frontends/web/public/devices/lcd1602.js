import { labeledPart, pin } from "./device.js";

const glyphs = {
  " ": [0,0,0,0,0,0,0], "?": [14,17,1,2,4,0,4],
  "0":[14,17,19,21,25,17,14], "1":[4,12,4,4,4,4,14],
  "2":[14,17,1,2,4,8,31], "3":[30,1,1,14,1,1,30],
  "4":[2,6,10,18,31,2,2], "5":[31,16,30,1,1,17,14],
  "6":[6,8,16,30,17,17,14], "7":[31,1,2,4,8,8,8],
  "8":[14,17,17,14,17,17,14], "9":[14,17,17,15,1,2,12],
  "A":[14,17,17,31,17,17,17], "B":[30,17,17,30,17,17,30],
  "C":[14,17,16,16,16,17,14], "D":[30,17,17,17,17,17,30],
  "E":[31,16,16,30,16,16,31], "F":[31,16,16,30,16,16,16],
  "G":[14,17,16,23,17,17,15], "H":[17,17,17,31,17,17,17],
  "I":[14,4,4,4,4,4,14], "J":[7,2,2,2,2,18,12],
  "K":[17,18,20,24,20,18,17], "L":[16,16,16,16,16,16,31],
  "M":[17,27,21,21,17,17,17], "N":[17,25,21,19,17,17,17],
  "O":[14,17,17,17,17,17,14], "P":[30,17,17,30,16,16,16],
  "Q":[14,17,17,17,21,18,13], "R":[30,17,17,30,20,18,17],
  "S":[15,16,16,14,1,1,30], "T":[31,4,4,4,4,4,4],
  "U":[17,17,17,17,17,17,14], "V":[17,17,17,17,17,10,4],
  "W":[17,17,17,21,21,21,10], "X":[17,17,10,4,10,17,17],
  "Y":[17,17,10,4,4,4,4], "Z":[31,1,2,4,8,16,31],
  "-":[0,0,0,31,0,0,0], "_":[0,0,0,0,0,0,31],
  ".":[0,0,0,0,0,12,12], ":":[0,12,12,0,12,12,0]
};

function cell(character) {
  const glyph = glyphs[character] || glyphs[character.toUpperCase()] || glyphs["?"];
  const dots = [];
  for (let row = 0; row < 8; row++) {
    const bits = glyph[row] || 0;
    for (let column = 0; column < 5; column++) {
      dots.push(`<i class="${bits & (1 << (4 - column)) ? "on" : ""}"></i>`);
    }
  }
  return `<span class="lcd-cell">${dots.join("")}</span>`;
}

export function lineHtml(text = "") {
  return text.padEnd(16).slice(0, 16).split("").map(cell).join("");
}

export default {
  type: "i2c-lcd1602",
  idPrefix: "lcd",
  defaultAttrs: { address: 0x27 },
  category: "输出器件",
  palette: {
    title: "I²C LCD1602", detail: "PCF8574 · HD44780",
    iconClass: "lcd-icon", iconText: "16×2"
  },
  className: "lcd1602-part",
  size: { width: 430, height: 154 },
  positionMode: "top-left",
  styles: `
    .lcd-icon {
      width:34px;height:22px;border:2px solid #5ea16c;border-radius:3px;
      background:#183522;color:#8fe69c;font:700 9px ui-monospace,monospace;
    }
    .lcd1602-part .part-body {
      width:430px;height:154px;padding:18px 18px 18px 35px;
      border:2px solid #4d915b;border-radius:5px;background:#1d5d35;
    }
    .lcd-bezel {
      height:100%;padding:10px 12px;border:7px solid #163e25;
      border-radius:4px;background:#a7ca52;box-shadow:inset 0 0 13px #456521;
    }
    .lcd-line {
      height:43px;display:grid;grid-template-columns:repeat(16,1fr);gap:2px;
    }
    .lcd-line + .lcd-line { margin-top:2px; }
    .lcd-cell {
      min-width:0;display:grid;grid-template-columns:repeat(5,1fr);
      grid-template-rows:repeat(8,1fr);gap:1px;padding:2px;
      border:1px solid #789a3b55;border-radius:2px;background:#9fc34b55;
    }
    .lcd-cell i {
      display:block;min-width:0;min-height:0;border-radius:50%;
      background:#789b4566;box-shadow:inset 0 0 1px #d5e989;
    }
    .lcd-cell i.on { background:#183f28;box-shadow:0 0 1px #183f28; }`,
  pins: [pin("SDA", "SDA", "left", 34), pin("SCL", "SCL", "left", 84)],
  render(part) {
    const address = Number(part.attrs?.address ?? 0x27).toString(16).toUpperCase();
    return labeledPart(`<div class="part-body">
      <div class="lcd-bezel"><div class="lcd-line" data-text="">${lineHtml()}</div>
      <div class="lcd-line" data-text="">${lineHtml()}</div></div></div>`,
    part.id, ` · 0x${address}`);
  }
};
