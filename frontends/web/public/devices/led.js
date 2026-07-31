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
