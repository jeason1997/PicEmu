import { pin } from "./device.js";

const pins = [
  pin("GP0", "1 GP0/ICSPDAT", "left", 38, 0),
  pin("VSS", "2 VSS", "left", 98),
  pin("GP1", "3 GP1/ICSPCLK", "left", 158, 1),
  pin("GP3", "6 GP3/MCLR/VPP", "right", 38, 3),
  pin("VDD", "5 VDD", "right", 98),
  pin("GP2", "4 GP2/T0CKI/FOSC4", "right", 158, 2)
];

function pic10(type, title, detail, order) {
  return {
    type,
    idPrefix: "mcu",
    category: "微控制器",
    categoryOrder: 0,
    palette: { title, detail, iconClass: "chip-icon", order },
    className: "mcu",
    size: { width: 300, height: 240 },
    positionMode: "top-left",
    pins,
    isMcu: true,
    deviceName: title,
    styles: `
      .chip-icon {
        background:#374151;border:2px solid #94a3b8;
        box-shadow:-5px 0 0 -3px #94a3b8,5px 0 0 -3px #94a3b8;
      }
      .mcu .part-body {
        width:300px;height:240px;background:#29313c;
        border:1px solid #94a3b8;border-radius:4px;
      }
      .mcu-title {
        text-align:center;font:700 24px ui-monospace,monospace;
        padding-top:14px;letter-spacing:1px;
      }`,
    render() {
      return `<div class="part-body">
        <i class="pin-one"></i><div class="mcu-title">${title}</div></div>`;
    },
    update(context, part, element) {
      const state = context.model.states.get(part.id);
      if (!state) return;
      element.querySelectorAll(".pin[data-pin^='GP']").forEach(pinElement => {
        const number = Number(pinElement.dataset.pin.slice(2));
        const input = number === 3 || (state.tris & (1 << number)) !== 0;
        const high = (state.gpio & (1 << number)) !== 0;
        pinElement.classList.toggle("gpio-input", input);
        pinElement.classList.toggle("gpio-output", !input);
        pinElement.classList.toggle("gpio-high", high);
        pinElement.classList.toggle("gpio-low", !high);
        pinElement.title = `${pinElement.dataset.pin}: ${
          input ? "输入" : "输出"}，电平 ${high ? "高" : "低"}`;
      });
    },
    inspectorHtml(context, part) {
      const firstMcu = context.mcuParts()[0];
      const firmware = part.attrs?.firmware ||
        (part === firstMcu ? context.model.diagram.firmware || "" : "");
      return `<div class="property-row"><label>HEX 固件</label>
        <input id="propFirmware" value="${firmware}" disabled>
        <button id="propHexBtn" type="button">为 ${part.id} 设置 HEX</button></div>`;
    },
    bindInspector(context, part) {
      context.$("#propHexBtn").addEventListener("click", () => {
        context.model.hexTargetId = part.id;
        context.selectActiveMcu(part.id);
        context.$("#hexInput").value = "";
        context.$("#hexInput").click();
      });
    },
    async rename(context, part, oldId) {
      const oldState = context.model.states.get(oldId);
      context.model.states.delete(oldId);
      if (oldState) context.model.states.set(part.id, { ...oldState, mcuId: part.id });
      const oldBreakpoints = context.model.breakpoints.get(oldId);
      context.model.breakpoints.delete(oldId);
      if (oldBreakpoints) context.model.breakpoints.set(part.id, oldBreakpoints);
      if (context.model.activeMcuId === oldId) context.model.activeMcuId = part.id;
      if (!part.attrs?.firmware) return;
      try {
        context.setState(await context.post("/api/load", {
          mcuId: part.id, firmware: part.attrs.firmware, device: title
        }));
      } catch (error) { context.message(error.message, true); }
    },
    remove(context, part) {
      context.model.states.delete(part.id);
      context.model.breakpoints.delete(part.id);
    }
  };
}

export const pic10f200 = pic10("pic10f200", "PIC10F200", "6-Pin MCU", 0);
export const pic10f202 = pic10("pic10f202", "PIC10F202", "512-Word MCU", 1);
export const devices = [pic10f200, pic10f202];
