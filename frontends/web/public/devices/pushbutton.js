import { labeledPart } from "./device.js";

export default {
  type: "pushbutton",
  idPrefix: "button",
  defaultAttrs: { activeLow: true },
  category: "输入器件",
  categoryOrder: 2,
  palette: {
    title: "按键", detail: "按住产生低电平",
    iconClass: "button-icon", iconText: "↧", order: 0
  },
  className: "button-part",
  size: { width: 110, height: 60 },
  pins: [{ name: "1", dynamic: true }],
  defaultPortSide: "left",
  styles: `
    .button-icon { border:2px solid #94a3b8;border-radius:4px; }
    .button-part .part-body {
      width:110px;height:60px;background:#303b49;border:2px solid #94a3b8;
      border-radius:8px;display:grid;place-items:center;
    }
    .button-cap {
      width:30px;height:20px;border-radius:5px;background:#66758a;
      box-shadow:0 4px 0 #364252;
    }
    .button-part.pressed .button-cap {
      transform:translateY(4px);box-shadow:none;background:#35d0a0;
    }`,
  render(part) {
    return labeledPart(`<div class="part-body"><span class="button-cap"></span>
      <i class="device-pin" data-pin="1"></i></div>`, part.id);
  },
  pointerDown(context, part, element) {
    part.pressed = true;
    element.classList.add("pressed");
  },
  pointerUp(context, part, element) {
    part.pressed = false;
    element.classList.remove("pressed");
  },
  contributeInput(context, part, mcuId, input) {
    const endpoint = `${part.id}:1`;
    const connection = context.model.diagram.connections.find(item =>
      item.includes(endpoint) &&
      item.some(value => value.startsWith(`${mcuId}:GP`)));
    if (!connection) return;
    const mcuEnd = connection.find(value => value.startsWith(`${mcuId}:GP`));
    const pinNumber = Number(mcuEnd.match(/GP(\d)/)?.[1]);
    if (!Number.isFinite(pinNumber)) return;
    input.inputMask |= 1 << pinNumber;
    if (!part.pressed) input.inputValues |= 1 << pinNumber;
  }
};
