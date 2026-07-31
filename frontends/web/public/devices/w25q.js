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
