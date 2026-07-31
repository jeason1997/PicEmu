import { pin } from "./device.js";

const pins = [
  pin("GP0", "1 GP0/ICSPDAT", "left", 38, 0),
  pin("VSS", "2 VSS", "left", 98),
  pin("GP1", "3 GP1/ICSPCLK", "left", 158, 1),
  pin("GP3", "6 GP3/MCLR/VPP", "right", 38, 3),
  pin("VDD", "5 VDD", "right", 98),
  pin("GP2", "4 GP2/T0CKI/FOSC4", "right", 158, 2)
];

function pic10(type, title, detail) {
  return {
    type,
    idPrefix: "mcu",
    category: "微控制器",
    palette: { title, detail, iconClass: "chip-icon" },
    className: "mcu",
    size: { width: 300, height: 240 },
    positionMode: "top-left",
    pins,
    isMcu: true,
    deviceName: title,
    render() {
      return `<div class="part-body">
        <i class="pin-one"></i><div class="mcu-title">${title}</div></div>`;
    }
  };
}

export const pic10f200 = pic10("pic10f200", "PIC10F200", "6-Pin MCU");
export const pic10f202 = pic10("pic10f202", "PIC10F202", "512-Word MCU");
