import { labeledPart, pin } from "./device.js";

export default {
  type: "seven-segment",
  defaultAttrs: { activeHigh: true, color: "red" },
  category: "输出器件",
  palette: {
    title: "七段数码管", detail: "a～g、dp 独立输入",
    iconClass: "seven-segment-icon", iconText: "8."
  },
  className: "seven-segment-part",
  size: { width: 150, height: 190 },
  positionMode: "top-left",
  pins: ["a","b","c","d","e","f","g","dp"].map(
    (name, index) => pin(name, name, "left", 9 + index * 22)
  ),
  render(part) {
    return labeledPart(`<div class="part-body"><div class="seven-display">
      ${["a","b","c","d","e","f","g","dp"].map(
        segment => `<i data-segment="${segment}"></i>`).join("")}
      </div></div>`, part.id);
  }
};
