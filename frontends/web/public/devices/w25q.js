import { labeledPart, pin } from "./device.js";

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
  palette: {
    title: "W25Q Flash", detail: "SPI NOR",
    iconClass: "flash-icon", iconText: "▦"
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
  }
};
