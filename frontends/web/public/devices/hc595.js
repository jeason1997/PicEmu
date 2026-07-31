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
  styles: `
    .hc595-icon {
      background:#1a293b;color:#8dc5ff;font:700 13px ui-monospace,monospace;
      border:2px solid #52749a;border-radius:4px;
    }
    .hc595-part .part-body {
      width:170px;height:205px;background:#26323f;border:3px solid #8797aa;
      border-radius:7px;box-shadow:inset 0 0 16px #0008;
    }
    .hc595-title {
      position:absolute;left:45px;top:85px;font:700 17px ui-monospace,monospace;
      color:#e5ecf5;
    }`,
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
