import { pic10f200, pic10f202 } from "./pic10.js";
import led from "./led.js";
import buzzer from "./buzzer.js";
import lcd1602 from "./lcd1602.js";
import sevenSegment from "./seven-segment.js";
import pushbutton from "./pushbutton.js";
import hc595 from "./hc595.js";
import w25q from "./w25q.js";

/*
 * 唯一的器件注册表。app.js 只面向这里公开的统一接口，不再包含具体器件模板。
 * 注册时立即检查重复类型，避免后加入的模块静默覆盖已有器件。
 */
const definitions = [
  pic10f200, pic10f202, led, buzzer, lcd1602,
  sevenSegment, pushbutton, hc595, w25q
];
const registry = new Map();
for (const definition of definitions) {
  if (registry.has(definition.type)) {
    throw new Error(`重复注册 Web 器件：${definition.type}`);
  }
  registry.set(definition.type, Object.freeze(definition));
}

export function deviceDefinition(type) {
  const definition = registry.get(type);
  if (!definition) throw new Error(`尚未注册 Web 器件：${type}`);
  return definition;
}

export function hasDevice(type) {
  return registry.has(type);
}

export function isMcuType(type) {
  return registry.get(type)?.isMcu === true;
}

export function renderPalette(container) {
  const groups = new Map();
  for (const definition of definitions) {
    if (!groups.has(definition.category)) groups.set(definition.category, []);
    groups.get(definition.category).push(definition);
  }
  container.innerHTML = [...groups].map(([category, devices]) => `
    <div class="palette-group"><h3>${category}</h3>
      ${devices.map(({ type, palette }) => `
        <div class="palette-item" draggable="true" data-type="${type}">
          <span class="part-icon ${palette.iconClass}">${palette.iconText || ""}</span>
          <div><b>${palette.title}</b><small>${palette.detail}</small></div>
        </div>`).join("")}
    </div>`).join("");
}

export function installDeviceStyles(documentRoot = document) {
  const styleId = "picemu-device-styles";
  let style = documentRoot.getElementById(styleId);
  if (!style) {
    style = documentRoot.createElement("style");
    style.id = styleId;
    documentRoot.head.appendChild(style);
  }
  /*
   * 样式与器件定义共置，但只生成一个 <style> 节点，避免每个器件产生额外
   * 网络请求或大量零散节点。重复调用时直接覆盖，便于开发阶段热重载。
   */
  style.textContent = [...new Set(
    definitions.map(definition => definition.styles).filter(Boolean)
  )].join("\n");
}
