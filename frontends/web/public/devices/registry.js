/*
 * 唯一的器件注册表。app.js 只面向这里公开的统一接口，不再包含具体器件模板。
 * 注册时立即检查重复类型，避免后加入的模块静默覆盖已有器件。
 */
const definitions = [];
const registry = new Map();

export async function initializeDeviceRegistry() {
  if (definitions.length > 0) return;
  const response = await fetch("/api/device-modules");
  const result = await response.json();
  if (!response.ok || !result.ok) {
    throw new Error(result.error || "无法读取 Web 器件模块列表");
  }
  for (const modulePath of result.modules) {
    const loaded = await import(modulePath);
    const candidates = loaded.devices ||
      (Array.isArray(loaded.default) ? loaded.default : [loaded.default]);
    for (const definition of candidates.filter(Boolean)) {
      if (registry.has(definition.type)) {
        throw new Error(`重复注册 Web 器件：${definition.type}`);
      }
      definitions.push(Object.freeze(definition));
      registry.set(definition.type, definition);
    }
  }
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
  const orderedGroups = [...groups].sort((a, b) =>
    Math.min(...a[1].map(device => device.categoryOrder ?? 999)) -
    Math.min(...b[1].map(device => device.categoryOrder ?? 999)));
  container.innerHTML = orderedGroups.map(([category, devices]) => `
    <div class="palette-group"><h3>${category}</h3>
      ${devices.sort((a, b) =>
        (a.palette.order ?? 999) - (b.palette.order ?? 999))
        .map(({ type, palette }) => `
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

export async function configureDevices(context, strict = true) {
  for (const part of context.model.diagram.parts) {
    const configure = deviceDefinition(part.type).configure;
    if (configure) await configure(context, part, strict);
  }
}

export function notifyDevices(context, hook, ...args) {
  for (const part of context.model.diagram.parts) {
    deviceDefinition(part.type)[hook]?.(context, part, ...args);
  }
}
