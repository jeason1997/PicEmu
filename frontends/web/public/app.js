import {
  configureDevices, deviceDefinition, hasDevice, installDeviceStyles,
  initializeDeviceRegistry, isMcuType, notifyDevices, renderPalette
} from "./devices/registry.js";

await initializeDeviceRegistry();

const $ = selector => document.querySelector(selector);
const stage = $("#stage");
const stageSizer = $("#stageSizer");
const stageViewport = $("#stageViewport");
const partsLayer = $("#partsLayer");
const wires = $("#wires");
/*
 * diagram.json 继续使用以电路为中心的普通坐标；画布内部额外增加一个很大的
 * 虚拟原点。这样既保持电路文件中的世界坐标，又能向四个方向持续平移。
 */
const STAGE_WIDTH = 12000;
const STAGE_HEIGHT = 8000;
const ORIGIN_X = STAGE_WIDTH / 2;
const ORIGIN_Y = STAGE_HEIGHT / 2;
const GRID_SIZE = 20;
const SNAP_SIZE = GRID_SIZE / 2;

const model = {
  diagram: { version: 1, clockHz: 4000000, firmware: "", parts: [], connections: [] },
  state: null,
  states: new Map(),
  activeMcuId: null,
  flash: [],
  instructions: [],
  breakpoints: new Map(),
  running: false,
  selected: null,
  selectedIds: new Set(),
  selectedConnection: null,
  pendingPin: null,
  lastFrame: performance.now(),
  budget: 0,
  requestPending: false,
  /*
   * 区分“刚加载/刚复位、尚未执行固件”和普通暂停。首次执行前需要重新应用
   * diagram.json 的外设初值，避免启动前的调试写入改变固件所见的上电状态。
   */
  executionStarted: false,
  zoom: 1,
  diagramName: "未命名电路",
  diagramPath: null,
  hexTargetId: null,
  history: [],
  historyIndex: -1
};

function mcuDevice(part) {
  return deviceDefinition(part.type).deviceName;
}

async function api(url, options = {}) {
  const response = await fetch(url, options);
  const result = await response.json();
  if (!response.ok || result.ok === false) {
    throw new Error(result.error || `HTTP ${response.status}`);
  }
  return result;
}

const post = (url, value) => api(url, {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify(value)
});

function message(text, error = false) {
  $("#message").textContent = text;
  $("#message").style.color = error ? "#ff6577" : "";
}

function hex(value, digits = 2) {
  return "0x" + Number(value || 0).toString(16).toUpperCase().padStart(digits, "0");
}

function pinKey(partId, pin) { return `${partId}:${pin}`; }
function snap(value) { return Math.round(value / SNAP_SIZE) * SNAP_SIZE; }
function stageX(worldX) { return worldX + ORIGIN_X; }
function stageY(worldY) { return worldY + ORIGIN_Y; }
function partSize(part) {
  const definition = deviceDefinition(part.type);
  return typeof definition.size === "function"
    ? definition.size(part)
    : definition.size;
}
function partTopLeft(part) {
  const size = partSize(part);
  if (deviceDefinition(part.type).positionMode === "top-left") {
    return { left: part.left, top: part.top };
  }
  return {
    left: part.left - size.width / 2,
    top: part.top - size.height / 2
  };
}
function mcuParts() {
  return model.diagram.parts.filter(part => isMcuType(part.type));
}
function activeBreakpoints() {
  const id = model.activeMcuId || "mcu";
  if (!model.breakpoints.has(id)) model.breakpoints.set(id, new Set());
  return model.breakpoints.get(id);
}

function selectActiveMcu(id) {
  if (!id || model.activeMcuId === id) return;
  model.activeMcuId = id;
  const state = model.states.get(id);
  if (state) setState(state);
}
function diagramSnapshot() {
  return JSON.stringify(model.diagram,
    (key, value) => key === "pressed" ? undefined : value);
}

function updateHistoryButtons() {
  $("#undoBtn").disabled = model.historyIndex <= 0;
  $("#redoBtn").disabled =
    model.historyIndex < 0 ||
    model.historyIndex >= model.history.length - 1;
  $("#restoreDraftBtn").disabled =
    localStorage.getItem("picemu.web.draft") === null;
}

function saveDraft() {
  localStorage.setItem("picemu.web.draft", JSON.stringify({
    name: model.diagramName,
    path: model.diagramPath,
    savedAt: new Date().toISOString(),
    diagram: JSON.parse(diagramSnapshot())
  }));
  updateHistoryButtons();
}

function resetHistory() {
  model.history = [diagramSnapshot()];
  model.historyIndex = 0;
  updateHistoryButtons();
}

function recordHistory() {
  const snapshot = diagramSnapshot();
  if (model.history[model.historyIndex] === snapshot) return;
  model.history.splice(model.historyIndex + 1);
  model.history.push(snapshot);
  if (model.history.length > 100) model.history.shift();
  else model.historyIndex++;
  saveDraft();
  updateHistoryButtons();
}

function moveHistory(offset) {
  const next = model.historyIndex + offset;
  if (next < 0 || next >= model.history.length) return;
  model.historyIndex = next;
  model.diagram = JSON.parse(model.history[next]);
  model.selected = null;
  model.selectedIds.clear();
  model.selectedConnection = null;
  renderAll();
  saveDraft();
  updateHistoryButtons();
  message(offset < 0 ? "已撤销" : "已重做");
}

function devicePortSide(part) {
  const definition = deviceDefinition(part.type);
  const pinName = definition.pins.find(pin => pin.dynamic)?.name;
  const endpoint = `${part.id}:${pinName}`;
  const connection = model.diagram.connections.find(item =>
    item[0] === endpoint || item[1] === endpoint);
  if (connection) {
    const otherEndpoint = connection[0] === endpoint
      ? connection[1] : connection[0];
    const otherId = otherEndpoint.split(":")[0];
    const other = model.diagram.parts.find(item => item.id === otherId);
    if (other) {
      const ownCenter = part.left;
      const otherPosition = partTopLeft(other);
      const otherSize = partSize(other);
      const otherCenter = otherPosition.left + otherSize.width / 2;
      return otherCenter < ownCenter ? "left" : "right";
    }
  }
  return definition.defaultPortSide;
}

function devicePortYOffset(part) {
  return partSize(part).height / 2;
}

function renderPart(part) {
  const definition = deviceDefinition(part.type);
  const el = document.createElement("div");
  el.className = `part ${definition.className}`;
  el.dataset.id = part.id;
  const position = partTopLeft(part);
  el.style.left = `${stageX(position.left)}px`;
  el.style.top = `${stageY(position.top)}px`;
  el.innerHTML = definition.render(part);
  definition.decorate?.(el, part);
  for (const pinDefinition of definition.pins.filter(pin => !pin.dynamic)) {
    const pin = document.createElement("div");
    pin.className = `pin ${pinDefinition.side}`;
    pin.style.top = `${pinDefinition.top}px`;
    pin.textContent = pinDefinition.label;
    pin.dataset.pin = pinDefinition.name;
    pin.addEventListener("pointerdown", event => {
      event.stopPropagation();
      selectPin(part, pinDefinition.name, pin);
    });
    el.querySelector(".part-body").appendChild(pin);
  }

  el.querySelectorAll(".device-pin").forEach(pin => {
    pin.classList.add(`port-${devicePortSide(part)}`);
    pin.style.top = `${devicePortYOffset(part) - 6}px`;
    pin.addEventListener("pointerdown", event => {
      event.stopPropagation(); selectPin(part, pin.dataset.pin, pin);
    });
  });
  el.addEventListener("pointerdown", event => beginPartDrag(event, part, el));
  partsLayer.appendChild(el);
}

function renderAll() {
  partsLayer.innerHTML = "";
  for (const part of model.diagram.parts) renderPart(part);
  updateSelection();
  requestAnimationFrame(renderWires);
  updatePartsFromState();
}

function pinPoint(endpoint) {
  const [id, pinName] = endpoint.split(":");
  const part = model.diagram.parts.find(item => item.id === id);
  if (!part) return null;
  const definition = deviceDefinition(part.type);
  if (definition.positionMode === "top-left") {
    const pin = definition.pins.find(item => item.name === pinName);
    if (!pin) return null;
    const size = partSize(part);
    const width = size.width;
    return {
      x: stageX(part.left) + (pin.side === "left" ? -6 : width + 6),
      y: stageY(part.top) + pin.top + 12,
      side: pin.side,
      top: stageY(part.top),
      bottom: stageY(part.top) + size.height
    };
  }
  const size = partSize(part);
  const side = devicePortSide(part);
  return {
    x: stageX(part.left) +
      (side === "left" ? -size.width / 2 : size.width / 2),
    y: stageY(part.top),
    side,
    top: stageY(part.top) - size.height / 2,
    bottom: stageY(part.top) + size.height / 2
  };
}

function automaticWirePath(a, b, lane = 0) {
  const stubLength = 30;
  const aStub = a.x + (a.side === "left" ? -stubLength : stubLength);
  const bStub = b.x + (b.side === "left" ? -stubLength : stubLength);
  const faceEachOther =
    (a.side === "right" && b.side === "left" && a.x < b.x) ||
    (a.side === "left" && b.side === "right" && b.x < a.x);

  if (faceEachOther) {
    const middle = snap((aStub + bStub) / 2);
    return `M${a.x},${a.y} H${middle} V${b.y} H${b.x}`;
  }

  /*
   * 引脚没有相向时，先沿引脚朝向离开器件，再从两个器件下方绕行，
   * 防止连线穿过器件本体而造成引脚归属不清。
   */
  const routeY = snap(Math.max(a.bottom, b.bottom) + 40 +
    (lane % 8) * 20);
  return [
    `M${a.x},${a.y}`,
    `H${aStub}`,
    `V${routeY}`,
    `H${bStub}`,
    `V${b.y}`,
    `H${b.x}`
  ].join(" ");
}

function renderWires() {
  wires.innerHTML = "";
  model.diagram.connections.forEach((connection, index) => {
    const a = pinPoint(connection[0]);
    const b = pinPoint(connection[1]);
    if (!a || !b) return;
    const configuredPoints = Array.isArray(connection[3])
      ? connection[3].map(point => Array.isArray(point)
        ? { x: Number(point[0]), y: Number(point[1]) }
        : { x: Number(point.x), y: Number(point.y) })
        .filter(point => Number.isFinite(point.x) && Number.isFinite(point.y))
      : [];
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("class", "wire");
    if (index === model.selectedConnection) path.classList.add("selected");
    path.setAttribute("stroke", connection[2] || "#60a5fa");
    if (configuredPoints.length > 0) {
      path.setAttribute("d", [
        `M${a.x},${a.y}`,
        ...configuredPoints.map(point =>
          `H${stageX(point.x)} V${stageY(point.y)}`),
        `H${b.x} V${b.y}`
      ].join(" "));
    } else {
      path.setAttribute("d", automaticWirePath(a, b, index));
    }
    path.addEventListener("pointerdown", event => {
      event.stopPropagation();
      model.selected = null;
      model.selectedIds.clear();
      model.selectedConnection = index;
      updateSelection();
      renderWires();
      message(`已选择连线 ${connection[0]} ↔ ${connection[1]}`);
    });
    wires.appendChild(path);
  });
}

function selectPin(part, pin, element) {
  model.selectedConnection = null;
  updateSelection();
  const endpoint = pinKey(part.id, pin);
  if (!model.pendingPin) {
    model.pendingPin = { endpoint, element };
    element.classList.add("connecting");
    message(`请选择要连接到 ${endpoint} 的另一个引脚`);
    return;
  }
  model.pendingPin.element.classList.remove("connecting");
  if (model.pendingPin.endpoint !== endpoint) {
    const exists = model.diagram.connections.some(c =>
      c.includes(model.pendingPin.endpoint) && c.includes(endpoint));
    if (!exists) {
      model.diagram.connections.push([
        model.pendingPin.endpoint, endpoint, "#55aaff", []
      ]);
      recordHistory();
    }
  }
  model.pendingPin = null;
  renderPortDirections();
  renderWires();
  message("连线已更新");
  configureDevices(deviceContext(), false)
    .catch(error => message(error.message, true));
}

function beginPartDrag(event, part, el) {
  if (event.button !== 0 || event.target.matches(".pin,.device-pin")) return;
  event.preventDefault();
  const additive = event.ctrlKey || event.metaKey || event.shiftKey;
  if (additive) {
    if (model.selectedIds.has(part.id)) {
      model.selectedIds.delete(part.id);
      model.selected = [...model.selectedIds].at(-1) || null;
      updateSelection();
      if (model.selectedIds.size === 1) {
        showPartInspector(model.diagram.parts.find(
          item => item.id === model.selected));
      }
      return;
    }
    model.selectedIds.add(part.id);
  } else if (!model.selectedIds.has(part.id)) {
    model.selectedIds.clear();
    model.selectedIds.add(part.id);
  }
  model.selected = part.id;
  model.selectedConnection = null;
  updateSelection();
  if (model.selectedIds.size === 1) showPartInspector(part);
  if (isMcuType(part.type)) selectActiveMcu(part.id);
  const startX = event.clientX, startY = event.clientY;
  const starts = new Map(model.diagram.parts
    .filter(item => model.selectedIds.has(item.id))
    .map(item => [item.id, { left: item.left, top: item.top }]));
  let finished = false;
  const definition = deviceDefinition(part.type);
  definition.pointerDown?.(deviceContext(), part, el);
  el.setPointerCapture(event.pointerId);
  const move = e => {
    const dx = snap((e.clientX - startX) / model.zoom);
    const dy = snap((e.clientY - startY) / model.zoom);
    for (const item of model.diagram.parts) {
      const origin = starts.get(item.id);
      if (!origin) continue;
      item.left = origin.left + dx;
      item.top = origin.top + dy;
      const itemElement = document.querySelector(
        `.part[data-id="${CSS.escape(item.id)}"]`);
      const position = partTopLeft(item);
      if (itemElement) {
        itemElement.style.left = `${stageX(position.left)}px`;
        itemElement.style.top = `${stageY(position.top)}px`;
      }
    }
    const positionInput = $("#propPosition");
    if (positionInput && model.selectedIds.size === 1) {
      positionInput.value = `${part.left}, ${part.top}`;
    }
    renderPortDirections();
    renderWires();
  };
  const up = () => {
    if (finished) return;
    finished = true;
    definition.pointerUp?.(deviceContext(), part, el);
    el.removeEventListener("pointermove", move);
    el.removeEventListener("pointerup", up);
    el.removeEventListener("pointercancel", up);
    window.removeEventListener("pointerup", up);
    const origin = starts.get(part.id);
    if (part.left !== origin.left || part.top !== origin.top) recordHistory();
  };
  el.addEventListener("pointermove", move);
  el.addEventListener("pointerup", up);
  el.addEventListener("pointercancel", up);
  window.addEventListener("pointerup", up);
}

function renderPortDirections() {
  for (const part of model.diagram.parts) {
    if (!deviceDefinition(part.type).pins.some(pin => pin.dynamic)) continue;
    const pin = document.querySelector(
      `.part[data-id="${CSS.escape(part.id)}"] .device-pin`);
    if (!pin) continue;
    pin.classList.remove("port-left", "port-right");
    pin.classList.add(`port-${devicePortSide(part)}`);
    pin.style.top = `${devicePortYOffset(part) - 6}px`;
  }
}

function updateSelection() {
  document.querySelectorAll(".part").forEach(el =>
    el.classList.toggle("selected", model.selectedIds.has(el.dataset.id)));
  document.querySelectorAll(".pin.wire-endpoint,.device-pin.wire-endpoint")
    .forEach(pin => pin.classList.remove("wire-endpoint"));
  const connection = model.diagram.connections[model.selectedConnection];
  if (!connection) return;
  for (const endpoint of connection.slice(0, 2)) {
    const separator = endpoint.indexOf(":");
    const id = endpoint.slice(0, separator);
    const pinName = endpoint.slice(separator + 1);
    const pin = document.querySelector(
      `.part[data-id="${CSS.escape(id)}"] [data-pin="${
        CSS.escape(pinName)}"]`);
    pin?.classList.add("wire-endpoint");
  }
}

function uniqueId(type) {
  const base = deviceDefinition(type).idPrefix || type;
  let index = 1, value = base;
  while (model.diagram.parts.some(part => part.id === value)) value = base + index++;
  return value;
}

installDeviceStyles();
renderPalette($("#devicePalette"));
document.querySelectorAll(".palette-item").forEach(item => {
  item.addEventListener("dragstart", event =>
    event.dataTransfer.setData("application/x-picemu-part", item.dataset.type));
});
stageViewport.addEventListener("dragover", event => event.preventDefault());
stageViewport.addEventListener("drop", event => {
  event.preventDefault();
  const type = event.dataTransfer.getData("application/x-picemu-part");
  if (!hasDevice(type)) return;
  const definition = deviceDefinition(type);
  const rect = stageViewport.getBoundingClientRect();
  const part = {
    id: uniqueId(type), type,
    left: snap(
      (stageViewport.scrollLeft + event.clientX - rect.left) / model.zoom -
      ORIGIN_X),
    top: snap(
      (stageViewport.scrollTop + event.clientY - rect.top) / model.zoom -
      ORIGIN_Y)
  };
  if (definition.defaultAttrs) part.attrs = { ...definition.defaultAttrs };
  model.diagram.parts.push(part);
  if (isMcuType(type)) selectActiveMcu(part.id);
  model.selectedIds.clear();
  model.selectedIds.add(part.id);
  model.selected = part.id; renderAll(); showPartInspector(part);
  recordHistory();
});

stageViewport.addEventListener("pointerdown", event => {
  if (event.button !== 0 ||
      event.target.closest?.(".part") ||
      event.target.closest?.(".wire")) return;
  event.preventDefault();
  const rect = stageViewport.getBoundingClientRect();
  const start = {
    x: (stageViewport.scrollLeft + event.clientX - rect.left) / model.zoom,
    y: (stageViewport.scrollTop + event.clientY - rect.top) / model.zoom
  };
  const additive = event.ctrlKey || event.metaKey || event.shiftKey;
  const marquee = document.createElement("div");
  marquee.className = "selection-marquee";
  marquee.style.left = `${start.x}px`;
  marquee.style.top = `${start.y}px`;
  stage.appendChild(marquee);
  stageViewport.setPointerCapture(event.pointerId);

  const move = e => {
    const x = (stageViewport.scrollLeft + e.clientX - rect.left) / model.zoom;
    const y = (stageViewport.scrollTop + e.clientY - rect.top) / model.zoom;
    marquee.style.left = `${Math.min(start.x, x)}px`;
    marquee.style.top = `${Math.min(start.y, y)}px`;
    marquee.style.width = `${Math.abs(x - start.x)}px`;
    marquee.style.height = `${Math.abs(y - start.y)}px`;
  };
  const up = e => {
    const x = (stageViewport.scrollLeft + e.clientX - rect.left) / model.zoom;
    const y = (stageViewport.scrollTop + e.clientY - rect.top) / model.zoom;
    const box = {
      left: Math.min(start.x, x) - ORIGIN_X,
      top: Math.min(start.y, y) - ORIGIN_Y,
      right: Math.max(start.x, x) - ORIGIN_X,
      bottom: Math.max(start.y, y) - ORIGIN_Y
    };
    if (!additive) model.selectedIds.clear();
    if (Math.abs(x - start.x) >= 3 || Math.abs(y - start.y) >= 3) {
      for (const part of model.diagram.parts) {
        const position = partTopLeft(part);
        const size = partSize(part);
        if (position.left <= box.right &&
            position.left + size.width >= box.left &&
            position.top <= box.bottom &&
            position.top + size.height >= box.top) {
          model.selectedIds.add(part.id);
        }
      }
    }
    model.selected = [...model.selectedIds].at(-1) || null;
    model.selectedConnection = null;
    if (model.pendingPin) {
      model.pendingPin.element.classList.remove("connecting");
      model.pendingPin = null;
    }
    marquee.remove();
    stageViewport.removeEventListener("pointermove", move);
    stageViewport.removeEventListener("pointerup", up);
    stageViewport.removeEventListener("pointercancel", cancel);
    updateSelection();
    renderWires();
    if (model.selectedIds.size === 1) {
      showPartInspector(model.diagram.parts.find(
        part => part.id === model.selected));
    } else if (model.selectedIds.size > 1) {
      message(`已选择 ${model.selectedIds.size} 个器件`);
    }
  };
  const cancel = () => {
    marquee.remove();
    stageViewport.removeEventListener("pointermove", move);
    stageViewport.removeEventListener("pointerup", up);
    stageViewport.removeEventListener("pointercancel", cancel);
  };
  stageViewport.addEventListener("pointermove", move);
  stageViewport.addEventListener("pointerup", up);
  stageViewport.addEventListener("pointercancel", cancel);
});

function setZoom(nextZoom, clientX, clientY) {
  const oldZoom = model.zoom;
  const zoom = Math.max(0.35, Math.min(2.5, nextZoom));
  if (zoom === oldZoom) return;
  const rect = stageViewport.getBoundingClientRect();
  const localX = clientX === undefined ? stageViewport.clientWidth / 2
    : clientX - rect.left;
  const localY = clientY === undefined ? stageViewport.clientHeight / 2
    : clientY - rect.top;
  const worldX = (stageViewport.scrollLeft + localX) / oldZoom;
  const worldY = (stageViewport.scrollTop + localY) / oldZoom;
  model.zoom = zoom;
  stage.style.transform = `scale(${zoom})`;
  stageSizer.style.width = `${STAGE_WIDTH * zoom}px`;
  stageSizer.style.height = `${STAGE_HEIGHT * zoom}px`;
  stageSizer.style.backgroundSize =
    `${GRID_SIZE * zoom}px ${GRID_SIZE * zoom}px`;
  stageViewport.scrollLeft = worldX * zoom - localX;
  stageViewport.scrollTop = worldY * zoom - localY;
  $("#zoomValue").textContent = `${Math.round(zoom * 100)}%`;
}

function fitDiagram() {
  if (model.diagram.parts.length === 0) {
    setZoom(1);
    stageViewport.scrollTo?.(0, 0);
    return;
  }
  let minX = Infinity, minY = Infinity, maxX = 0, maxY = 0;
  for (const part of model.diagram.parts) {
    const size = partSize(part);
    const position = partTopLeft(part);
    minX = Math.min(minX, position.left);
    minY = Math.min(minY, position.top);
    maxX = Math.max(maxX, position.left + size.width);
    maxY = Math.max(maxY, position.top + size.height);
  }
  const margin = 80;
  const zoom = Math.min(1,
    (stageViewport.clientWidth - margin) / (maxX - minX),
    (stageViewport.clientHeight - margin) / (maxY - minY));
  setZoom(Math.max(0.35, zoom));
  stageViewport.scrollLeft = stageX((minX + maxX) / 2) * model.zoom -
    stageViewport.clientWidth / 2;
  stageViewport.scrollTop = stageY((minY + maxY) / 2) * model.zoom -
    stageViewport.clientHeight / 2;
}

stageViewport.addEventListener("wheel", event => {
  if (event.deltaY === 0) return;
  event.preventDefault();
  setZoom(model.zoom * (event.deltaY < 0 ? 1.1 : 1 / 1.1),
    event.clientX, event.clientY);
}, { passive: false });

stageViewport.addEventListener("pointerdown", event => {
  if (event.button !== 1) return;
  event.preventDefault();
  const startX = event.clientX;
  const startY = event.clientY;
  const scrollLeft = stageViewport.scrollLeft;
  const scrollTop = stageViewport.scrollTop;
  stageViewport.classList.add("panning");
  stageViewport.setPointerCapture(event.pointerId);
  const move = e => {
    stageViewport.scrollLeft = scrollLeft - (e.clientX - startX);
    stageViewport.scrollTop = scrollTop - (e.clientY - startY);
  };
  const up = () => {
    stageViewport.classList.remove("panning");
    stageViewport.removeEventListener("pointermove", move);
    stageViewport.removeEventListener("pointerup", up);
    stageViewport.removeEventListener("pointercancel", up);
  };
  stageViewport.addEventListener("pointermove", move);
  stageViewport.addEventListener("pointerup", up);
  stageViewport.addEventListener("pointercancel", up);
});

$("#zoomOutBtn").addEventListener("click", () => setZoom(model.zoom / 1.2));
$("#zoomInBtn").addEventListener("click", () => setZoom(model.zoom * 1.2));
$("#zoomResetBtn").addEventListener("click", () => setZoom(1));
$("#zoomFitBtn").addEventListener("click", fitDiagram);

function inputState(mcuId = model.activeMcuId) {
  const input = { inputMask: 0, inputValues: 0 };
  const context = deviceContext();
  for (const part of model.diagram.parts) {
    deviceDefinition(part.type).contributeInput?.(context, part, mcuId, input);
  }
  return input;
}

async function command(commandName, extra = {}, mcuId = model.activeMcuId) {
  const result = await post("/api/command", {
    command: commandName, mcuId, ...inputState(mcuId), ...extra
  });
  setState(result);
  return result;
}

function setState(state) {
  const mcuId = state.mcuId || model.activeMcuId || "mcu";
  const merged = { ...(model.states.get(mcuId) || {}), ...state, mcuId };
  model.states.set(mcuId, merged);
  if (!model.activeMcuId) model.activeMcuId = mcuId;
  if (mcuId !== model.activeMcuId) {
    updatePartsFromState();
    return;
  }
  model.state = merged;
  $("#activeMcuLabel").textContent = mcuId;
  if (merged.flash) model.flash = merged.flash;
  if (merged.instructions) model.instructions = merged.instructions;
  state = merged;
  const instructionHz = Number(model.diagram.clockHz || 4000000) / 4;
  $("#simTime").textContent =
    `${(state.cycles / instructionHz * 1000).toFixed(3)} ms`;
  $("#cycleCount").textContent = Number(state.cycles).toLocaleString();
  $("#footerPc").textContent = hex(state.pc, 3);
  $("#pcValue").textContent = hex(state.pc, 3);
  const registers = [
    ["W", state.w], ["STATUS", state.status], ["TMR0", state.tmr0],
    ["FSR", state.fsr], ["OSCCAL", state.osccal], ["GPIO", state.gpio],
    ["TRIS", state.tris], ["OPTION", state.option]
  ];
  $("#registerGrid").innerHTML = registers.map(([name,value]) =>
    `<div class="register"><small>${name}</small><b>${hex(value)}</b></div>`).join("");
  $("#flags").innerHTML = ["C","DC","Z","PD","TO"].map((name, bit) =>
    `<span class="flag ${state.status & (1 << bit) ? "on" : ""}">${name}</span>`).join("");
  $("#stackView").innerHTML = state.stack.map((value,index) =>
    `<div class="stack-entry">STACK ${index} <b>${hex(value,3)}</b></div>`).join("");
  $("#gpioStates").innerHTML = [0, 1, 2, 3].map(pin => {
    const input = pin === 3 || (state.tris & (1 << pin)) !== 0;
    const high = (state.gpio & (1 << pin)) !== 0;
    return `<div class="gpio-state"><b>GP${pin}</b>
      <span class="${input ? "input" : "output"}">${
        input ? "IN" : "OUT"}</span>
      <span class="${high ? "high" : "low"}">${high ? "1" : "0"}</span></div>`;
  }).join("");
  $("#ramGrid").innerHTML = state.ram.map((value,index) =>
    `<div class="mem-cell ${index <= 6 ? "sfr" : ""}" title="${hex(index)}">${hex(value).slice(2)}</div>`).join("");
  renderFlash();
  updatePartsFromState();
}

function renderFlash() {
  const grid = $("#flashGrid");
  if (grid.children.length !== model.flash.length) {
    grid.innerHTML = model.flash.map((value,index) =>
      `<div class="flash-word" data-address="${index}">
        <span class="address">${hex(index,3)}</span>
        <span class="opcode">${hex(value,3).slice(2)}</span>
        <span class="assembly">${model.instructions[index] || "—"}</span>
      </div>`).join("");
  }
  grid.querySelector(".current")?.classList.remove("current");
  const current = grid.querySelector(`[data-address="${model.state?.pc}"]`);
  if (current) current.classList.add("current");
  grid.querySelectorAll(".flash-word").forEach(row => {
    row.classList.toggle("breakpoint",
      activeBreakpoints().has(Number(row.dataset.address)));
  });
  if (current && $("#followPc").checked &&
      document.querySelector('[data-tab="flash"]').classList.contains("active")) {
    grid.scrollTop = current.offsetTop -
      grid.clientHeight / 2 + current.offsetHeight / 2;
  }
}

$("#flashGrid").addEventListener("click", async event => {
  const row = event.target.closest(".flash-word");
  if (!row) return;
  const address = Number(row.dataset.address);
  const breakpoints = activeBreakpoints();
  if (breakpoints.has(address)) breakpoints.delete(address);
  else breakpoints.add(address);
  renderFlash();
  try {
    await command("breakpoints", {
      addresses: [...breakpoints].sort((a, b) => a - b)
    });
    message(`${hex(address, 3)} 断点已${
      breakpoints.has(address) ? "设置" : "取消"}`);
  } catch (error) {
    message(error.message, true);
  }
});

function connectedMcuPin(part, pinName) {
  const endpoint = `${part.id}:${pinName}`;
  const connection = model.diagram.connections.find(c => c.includes(endpoint));
  if (!connection) return null;
  const other = connection.find(value => value !== endpoint && /:GP\d$/.test(value));
  if (!other) return null;
  const [mcuId, mcuPinName] = other.split(":");
  return {
    mcuId,
    gpio: Number(mcuPinName.match(/GP(\d)$/)[1]),
    state: model.states.get(mcuId)
  };
}

function updatePartsFromState() {
  if (model.states.size === 0) return;
  const context = deviceContext();
  for (const part of model.diagram.parts) {
    const el = document.querySelector(`.part[data-id="${CSS.escape(part.id)}"]`);
    if (!el) continue;
    deviceDefinition(part.type).update?.(context, part, el);
  }
}

async function loadDiagram(diagram, name = "电路", diagramPath = null) {
  pause();
  const previousSelection = model.selected;
  notifyDevices(deviceContext(), "diagramUnload");
  $("#partInspector").innerHTML = "<p>选择器件以查看和编辑属性。</p>";
  model.diagram = diagram;
  model.diagramName = name;
  model.diagramPath = diagramPath;
  model.states.clear();
  model.state = null;
  model.activeMcuId = null;
  model.executionStarted = false;
  model.breakpoints.clear();
  model.selectedConnection = null;
  model.diagram.parts ||= [];
  model.diagram.connections ||= [];
  /*
   * 重新打开同一示例时保留仍然存在的器件选择。属性面板会绑定到新 diagram
   * 对象中的实例，既不显示旧快照，也不要求用户先选择其他器件再选回来。
   */
  model.selected = model.diagram.parts.some(
    part => part.id === previousSelection) ? previousSelection : null;
  model.selectedIds.clear();
  if (model.selected) model.selectedIds.add(model.selected);
  $("#diagramName").textContent = name;
  $("#saveDiagramBtn").disabled = diagramPath === null;
  renderAll();
  if (model.selected) {
    showPartInspector(model.diagram.parts.find(
      part => part.id === model.selected));
  }
  resetHistory();
  requestAnimationFrame(fitDiagram);
  const mcus = mcuParts();
  if (mcus.length === 0) {
    message("电路已载入；请添加 PIC 芯片");
    return;
  }
  model.activeMcuId = mcus[0].id;
  let requestedLoads = 0;
  const loads = mcus.map((mcu, index) => {
    mcu.attrs ||= {};
    const firmware = mcu.attrs.firmware ||
      (index === 0 ? diagram.firmware : "");
    if (!firmware) return Promise.resolve(null);
    requestedLoads++;
    mcu.attrs.firmware = firmware;
    return post("/api/load", {
      mcuId: mcu.id,
      firmware,
      device: mcuDevice(mcu)
    });
  });
  if (requestedLoads === 0) {
    message("电路已载入；请设置HEX固件");
    return;
  }
  try {
    const states = await Promise.all(loads);
    states.filter(Boolean).forEach(setState);
    await configureDevices(deviceContext(), true);
    const loaded = states.filter(Boolean).length;
    const summaries = [];
    for (const type of new Set(model.diagram.parts.map(part => part.type))) {
      const definition = deviceDefinition(type);
      const parts = model.diagram.parts.filter(part => part.type === type);
      const summary = definition.loadSummary?.(parts);
      if (summary) summaries.push(summary);
    }
    message(`已加载 ${loaded} 个 MCU 固件${summaries.join("")}`);
  } catch (error) {
    message(`固件加载失败：${error.message}`, true);
  }
}

async function openExample() {
  const path = $("#exampleSelect").value;
  if (!path) return;
  const diagram = await api(`/api/file?path=${encodeURIComponent(path)}`);
  await loadDiagram(diagram, path.split("/")[1], path);
}

function pause() {
  model.running = false;
  $("#runState").textContent = "已暂停";
  $("#runState").className = "status paused";
  updatePartsFromState();
}

async function prepareFirstExecution() {
  if (model.executionStarted) return;
  /*
   * 属性面板操作可能改变运行期状态。固件首次运行或单步前，让各器件自行
   * 恢复电路文件定义的上电配置，避免调试操作污染固件看到的初始环境。
   */
  await configureDevices(deviceContext(), true);
  model.executionStarted = true;
}

$("#runBtn").addEventListener("click", async () => {
  if (![...model.states.values()].some(state => state.loaded)) {
    return message("请先为至少一个芯片加载HEX固件", true);
  }
  try {
    await prepareFirstExecution();
    model.running = true;
    model.lastFrame = performance.now();
    $("#runState").textContent = "运行中";
    $("#runState").className = "status running";
  } catch (error) {
    message(error.message, true);
  }
});
$("#pauseBtn").addEventListener("click", pause);
$("#stepBtn").addEventListener("click", async () => {
  pause();
  try {
    await prepareFirstExecution();
    const before = model.state?.pc;
    const instruction = Number.isInteger(before)
      ? model.instructions[before] || "未知指令" : "未知指令";
    const result = await command("step");
    message(`单步 ${hex(before, 3)} ${instruction} → PC ${hex(result.pc, 3)}`);
  } catch (error) { message(error.message, true); }
});
$("#resetBtn").addEventListener("click", async () => {
  pause();
  notifyDevices(deviceContext(), "diagramUnload");
  try {
    await Promise.all([...model.states.entries()]
      .filter(([, state]) => state.loaded)
      .map(([id]) => command("reset", {}, id)));
    /* 复位后由各器件重新应用 diagram.json 中属于自己的上电配置。 */
    await configureDevices(deviceContext(), true);
    model.executionStarted = false;
    message("程序和外设已复位");
  } catch (error) { message(error.message, true); }
});

/*
 * 器件只能通过这个受控上下文访问电路和通用服务。新增器件无需从 app.js
 * 导入私有变量，也不会反向形成循环依赖。
 */
function deviceContext() {
  return {
    $, model, post, message, hex, mcuParts, setState, renderAll,
    recordHistory, showPartInspector, selectActiveMcu, connectedMcuPin
  };
}

async function frame(now) {
  const elapsed = Math.min(0.1, (now - model.lastFrame) / 1000);
  model.lastFrame = now;
  const loadedIds = [...model.states.entries()]
    .filter(([, state]) => state.loaded)
    .map(([id]) => id);
  if (model.running && loadedIds.length > 0 && !model.requestPending) {
    const speed = Number($("#speedSelect").value);
    const instructionHz = Number(model.diagram.clockHz || 4000000) / 4;
    model.budget += elapsed * instructionHz * speed;
    const cycles = Math.min(100000, Math.floor(model.budget));
    if (cycles > 0) {
      model.budget -= cycles; model.requestPending = true;
      try {
        const states = await Promise.all(
          loadedIds.map(id => command("run", { cycles }, id)));
        const breakpoint = states.find(state => state.breakpointHit);
        const stopped = states.find(state => state.stopped);
        if (breakpoint) {
          pause();
          message(`${breakpoint.mcuId} 断点命中：${hex(breakpoint.pc, 3)}`);
        } else if (stopped) {
          pause();
          message(`${stopped.mcuId}: ${
            stopped.stopReason || "CPU已停止"}`, true);
        }
      } catch (error) {
        pause(); message(error.message, true);
      } finally {
        model.requestPending = false;
      }
    }
  }
  notifyDevices(deviceContext(), "frame", now);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

function showPartInspector(part) {
  const context = deviceContext();
  const definition = deviceDefinition(part.type);
  $("#partInspector").innerHTML = `
    <div class="property-row"><label>ID</label><input id="propId" value="${part.id}"></div>
    <div class="property-row"><label>类型</label><input value="${part.type}" disabled></div>
    ${definition.inspectorHtml?.(context, part) || ""}
    <div class="property-row"><label>位置</label>
      <input id="propPosition" value="${part.left}, ${part.top}" disabled></div>`;
  $("#propId").addEventListener("change", async event => {
    const oldId = part.id;
    const newId = event.target.value.trim();
    if (!newId || model.diagram.parts.some(item =>
      item !== part && item.id === newId)) {
      event.target.value = oldId;
      return;
    }
    part.id = newId;
    model.diagram.connections.forEach(connection => {
      connection[0] = connection[0].replace(`${oldId}:`, `${newId}:`);
      connection[1] = connection[1].replace(`${oldId}:`, `${newId}:`);
    });
    await definition.rename?.(context, part, oldId);
    model.selectedIds.delete(oldId);
    model.selectedIds.add(newId);
    model.selected = newId;
    renderAll();
    recordHistory();
  });
  definition.bindInspector?.(context, part);
}

document.querySelectorAll(".tab").forEach(tab => tab.addEventListener("click", () => {
  document.querySelectorAll(".tab,.tab-panel").forEach(item => item.classList.remove("active"));
  tab.classList.add("active");
  $(`#${tab.dataset.tab}Panel`).classList.add("active");
  if (tab.dataset.tab === "flash") renderFlash();
}));

$("#deleteBtn").addEventListener("click", () => {
  if (model.selectedConnection !== null) {
    model.diagram.connections.splice(model.selectedConnection, 1);
    model.selectedConnection = null;
    updateSelection();
    renderPortDirections();
    renderWires();
    message("已删除连线");
    recordHistory();
    return;
  }
  if (model.selectedIds.size === 0) return;
  const deletedIds = new Set(model.selectedIds);
  const deleted = model.diagram.parts.filter(part =>
    deletedIds.has(part.id));
  model.diagram.parts = model.diagram.parts.filter(part =>
    !deletedIds.has(part.id));
  model.diagram.connections = model.diagram.connections.filter(connection =>
    !connection.slice(0, 2).some(endpoint => [...deletedIds].some(
      id => endpoint.startsWith(id + ":"))));
  for (const part of deleted) {
    deviceDefinition(part.type).remove?.(deviceContext(), part);
  }
  if (deletedIds.has(model.activeMcuId)) {
    model.activeMcuId = mcuParts()[0]?.id || null;
    model.state = model.activeMcuId
      ? model.states.get(model.activeMcuId) || null : null;
    if (model.state) setState(model.state);
  }
  model.selectedIds.clear();
  model.selected = null;
  renderAll();
  message(`已删除 ${deleted.length} 个器件及相关连线`);
  recordHistory();
});
$("#undoBtn").addEventListener("click", () => moveHistory(-1));
$("#redoBtn").addEventListener("click", () => moveHistory(1));
$("#restoreDraftBtn").addEventListener("click", async () => {
  try {
    const draft = JSON.parse(localStorage.getItem("picemu.web.draft"));
    await loadDiagram(draft.diagram, `${draft.name || "电路"}（草稿）`,
      draft.path || null);
    message(`已恢复草稿：${new Date(draft.savedAt).toLocaleString()}`);
  } catch (error) {
    message(`恢复草稿失败：${error.message}`, true);
  }
});
window.addEventListener("keydown", event => {
  if (event.target.matches("input,select,textarea")) return;
  if ((event.ctrlKey || event.metaKey) &&
      event.key.toLowerCase() === "z") {
    event.preventDefault();
    moveHistory(event.shiftKey ? 1 : -1);
  } else if ((event.ctrlKey || event.metaKey) &&
             event.key.toLowerCase() === "y") {
    event.preventDefault();
    moveHistory(1);
  } else if (event.key === "Delete" || event.key === "Backspace") {
    event.preventDefault();
    $("#deleteBtn").click();
  } else if (event.key === "Escape" && model.pendingPin) {
    model.pendingPin.element.classList.remove("connecting");
    model.pendingPin = null;
    message("已取消连线");
  }
});
$("#loadExampleBtn").addEventListener("click", () => openExample().catch(e => message(e.message,true)));
$("#openDiagramBtn").addEventListener("click", () => $("#diagramInput").click());
$("#diagramInput").addEventListener("change", async event => {
  const file = event.target.files[0];
  if (file) await loadDiagram(JSON.parse(await file.text()), file.name);
});
$("#hexInput").addEventListener("change", async event => {
  const file = event.target.files[0];
  const mcu = mcuParts().find(part => part.id === model.hexTargetId);
  if (!file || !mcu) return;
  try {
    const state = await post("/api/upload-firmware", {
      mcuId: mcu.id,
      text: await file.text(),
      device: mcuDevice(mcu)
    });
    mcu.attrs ||= {};
    mcu.attrs.firmware = state.firmware;
    if (mcu === mcuParts()[0]) model.diagram.firmware = state.firmware;
    recordHistory();
    setState(state);
    showPartInspector(mcu);
    message(`${mcu.id} 已加载本地固件 ${file.name}`);
  } catch (error) { message(error.message, true); }
});
$("#saveDiagramBtn").addEventListener("click", async () => {
  if (!model.diagramPath) {
    return message("当前电路不是项目内示例，无法写回原文件", true);
  }
  try {
    const firstMcu = mcuParts()[0];
    if (firstMcu?.attrs?.firmware) {
      model.diagram.firmware = firstMcu.attrs.firmware;
    }
    const diagram = JSON.parse(diagramSnapshot());
    await post("/api/save-diagram", {
      path: model.diagramPath,
      diagram
    });
    resetHistory();
    message(`已保存：${model.diagramPath}`);
  } catch (error) {
    message(`保存失败：${error.message}`, true);
  }
});
$("#partSearch").addEventListener("input", event => {
  const query = event.target.value.toLowerCase();
  document.querySelectorAll(".palette-item").forEach(item =>
    item.style.display = item.textContent.toLowerCase().includes(query) ? "" : "none");
});

async function init() {
  updateHistoryButtons();
  const result = await api("/api/examples");
  $("#exampleSelect").innerHTML = result.examples.map(example =>
    `<option value="${example.diagram}" ${example.name === result.initialExample ? "selected" : ""}>${example.name}</option>`).join("");
  await openExample();
}
init().catch(error => message(error.message, true));
