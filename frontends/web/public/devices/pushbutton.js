import { labeledPart } from "./device.js";

export default {
  type: "pushbutton",
  idPrefix: "button",
  defaultAttrs: { activeLow: true },
  category: "输入器件",
  palette: {
    title: "按键", detail: "按住产生低电平",
    iconClass: "button-icon", iconText: "↧"
  },
  className: "button-part",
  size: { width: 110, height: 60 },
  pins: [{ name: "1", dynamic: true }],
  defaultPortSide: "left",
  render(part) {
    return labeledPart(`<div class="part-body"><span class="button-cap"></span>
      <i class="device-pin" data-pin="1"></i></div>`, part.id);
  }
};
