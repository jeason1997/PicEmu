import { labeledPart, pin } from "./device.js";

export default {
  type: "hc595",
  idPrefix: "shift",
  category: "存储器",
  palette: {
    title: "74HC595", detail: "8位串入并出",
    iconClass: "hc595-icon", iconText: "595"
  },
  className: "hc595-part",
  size: { width: 170, height: 205 },
  positionMode: "top-left",
  pins: [
    pin("SER", "SER", "left", 24),
    pin("SRCLK", "SRCLK", "left", 74),
    pin("RCLK", "RCLK", "left", 124),
    ...Array.from({ length: 8 }, (_, index) =>
      pin(`Q${index}`, `Q${index}`, "right", 8 + index * 22))
  ],
  render(part) {
    return labeledPart(`<div class="part-body"><i class="pin-one"></i>
      <div class="hc595-title">74HC595</div></div>`, part.id);
  }
};
