import { labeledPart } from "./device.js";

export default {
  type: "led",
  defaultAttrs: { color: "red" },
  category: "输出器件",
  palette: {
    title: "LED", detail: "红 / 绿 / 蓝 / 黄", iconClass: "led-icon"
  },
  className: "led-part",
  size: { width: 48, height: 48 },
  pins: [{ name: "A", dynamic: true }],
  defaultPortSide: "right",
  styles: `
    .led-icon {
      width:25px;height:25px;border-radius:50%;background:#ff4d55;
      box-shadow:0 0 10px #ff4d5577;
    }
    .led-part .part-body {
      width:48px;height:48px;border-radius:50%;
      background:#500d12;border:3px solid #a52b34;
    }
    .led-part.lit .part-body {
      background:#ff313d;box-shadow:0 0 28px #ff3344,inset 0 0 13px #fff8;
    }
    .led-part.green .part-body { background:#0b451f;border-color:#238948; }
    .led-part.green.lit .part-body {
      background:#24d45c;box-shadow:0 0 28px #24d45c;
    }
    .led-part.blue .part-body { background:#102b52;border-color:#2864a9; }
    .led-part.blue.lit .part-body {
      background:#3b9cff;box-shadow:0 0 28px #3b9cff;
    }
    .led-part.yellow .part-body { background:#4d3d08;border-color:#a9871c; }
    .led-part.yellow.lit .part-body {
      background:#ffd43b;box-shadow:0 0 28px #ffd43b;
    }`,
  decorate(element, part) {
    element.classList.add(part.attrs?.color || "red");
  },
  render(part) {
    return labeledPart(
      `<div class="part-body"><i class="device-pin" data-pin="A"></i></div>`,
      part.id
    );
  }
};
