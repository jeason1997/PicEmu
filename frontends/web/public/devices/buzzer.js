import { labeledPart } from "./device.js";

export default {
  type: "buzzer",
  category: "输出器件",
  palette: {
    title: "蜂鸣器", detail: "无源蜂鸣器",
    iconClass: "buzzer-icon", iconText: "◉"
  },
  className: "buzzer-part",
  size: { width: 60, height: 60 },
  pins: [{ name: "1", dynamic: true }],
  defaultPortSide: "left",
  render(part) {
    return labeledPart(
      `<div class="part-body">◉<i class="device-pin" data-pin="1"></i></div>`,
      part.id
    );
  }
};
