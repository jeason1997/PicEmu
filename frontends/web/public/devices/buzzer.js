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
  styles: `
    .buzzer-icon {
      border-radius:50%;border:2px solid #94a3b8;font-size:18px;
    }
    .buzzer-part .part-body {
      width:60px;height:60px;border-radius:50%;background:#242d39;
      border:3px solid #94a3b8;display:grid;place-items:center;font-size:22px;
    }`,
  render(part) {
    return labeledPart(
      `<div class="part-body">◉<i class="device-pin" data-pin="1"></i></div>`,
      part.id
    );
  }
};
