const $ = selector => document.querySelector(selector);
const stage = $("#stage");
const stageSizer = $("#stageSizer");
const stageViewport = $("#stageViewport");
const partsLayer = $("#partsLayer");
const wires = $("#wires");
/*
 * diagram.json 继续使用以电路为中心的普通坐标；画布内部额外增加一个很大的
 * 虚拟原点。这样既不破坏 SDL 共用的坐标，又能向四个方向持续平移。
 */
const STAGE_WIDTH = 12000;
const STAGE_HEIGHT = 8000;
const ORIGIN_X = STAGE_WIDTH / 2;
const ORIGIN_Y = STAGE_HEIGHT / 2;
const GRID_SIZE = 20;
const SNAP_SIZE = GRID_SIZE / 2;
const W25_CAPACITIES = [
  [131072, "W25Q10 · 128 KiB"],
  [262144, "W25Q20 · 256 KiB"],
  [524288, "W25Q40 · 512 KiB"],
  [1048576, "W25Q80 · 1 MiB"],
  [2097152, "W25Q16 · 2 MiB"],
  [4194304, "W25Q32 · 4 MiB"],
  [8388608, "W25Q64 · 8 MiB"],
  [16777216, "W25Q128 · 16 MiB"]
];
const MCU_TYPES = new Set(["pic10f200", "pic10f202"]);
const LCD_GLYPHS = {
  " ": [0,0,0,0,0,0,0], "?": [14,17,1,2,4,0,4],
  "0":[14,17,19,21,25,17,14], "1":[4,12,4,4,4,4,14],
  "2":[14,17,1,2,4,8,31], "3":[30,1,1,14,1,1,30],
  "4":[2,6,10,18,31,2,2], "5":[31,16,30,1,1,17,14],
  "6":[6,8,16,30,17,17,14], "7":[31,1,2,4,8,8,8],
  "8":[14,17,17,14,17,17,14], "9":[14,17,17,15,1,2,12],
  "A":[14,17,17,31,17,17,17], "B":[30,17,17,30,17,17,30],
  "C":[14,17,16,16,16,17,14], "D":[30,17,17,17,17,17,30],
  "E":[31,16,16,30,16,16,31], "F":[31,16,16,30,16,16,16],
  "G":[14,17,16,23,17,17,15], "H":[17,17,17,31,17,17,17],
  "I":[14,4,4,4,4,4,14], "J":[7,2,2,2,2,18,12],
  "K":[17,18,20,24,20,18,17], "L":[16,16,16,16,16,16,31],
  "M":[17,27,21,21,17,17,17], "N":[17,25,21,19,17,17,17],
  "O":[14,17,17,17,17,17,14], "P":[30,17,17,30,16,16,16],
  "Q":[14,17,17,17,21,18,13], "R":[30,17,17,30,20,18,17],
  "S":[15,16,16,14,1,1,30], "T":[31,4,4,4,4,4,4],
  "U":[17,17,17,17,17,17,14], "V":[17,17,17,17,17,10,4],
  "W":[17,17,17,21,21,21,10], "X":[17,17,10,4,10,17,17],
  "Y":[17,17,10,4,4,4,4], "Z":[31,1,2,4,8,16,31],
  "-":[0,0,0,31,0,0,0], "_":[0,0,0,0,0,0,31],
  ".":[0,0,0,0,0,12,12], ":":[0,12,12,0,12,12,0]
};

function lcdCellHtml(character) {
  const glyph = LCD_GLYPHS[character] ||
    LCD_GLYPHS[character.toUpperCase()] || LCD_GLYPHS["?"];
  const dots = [];
  for (let row = 0; row < 8; row++) {
    const bits = glyph[row] || 0;
    for (let column = 0; column < 5; column++) {
      dots.push(`<i class="${bits & (1 << (4 - column)) ? "on" : ""}"></i>`);
    }
  }
  return `<span class="lcd-cell">${dots.join("")}</span>`;
}

function lcdLineHtml(text = "") {
  return text.padEnd(16).slice(0, 16).split("").map(lcdCellHtml).join("");
}

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
  audio: null,
  oscillator: null,
  gain: null,
  zoom: 1,
  diagramName: "未命名电路",
  diagramPath: null,
  hexTargetId: null,
  history: [],
  historyIndex: -1,
  w25Offsets: new Map(),
  w25RefreshPending: false,
  w25LastRefresh: 0
};

const pinDefs = {
  pic10f200: [
    { name:"GP0", label:"1 GP0/ICSPDAT", side:"left", top:38, gpio:0 },
    { name:"VSS", label:"2 VSS", side:"left", top:98 },
    { name:"GP1", label:"3 GP1/ICSPCLK", side:"left", top:158, gpio:1 },
    { name:"GP3", label:"6 GP3/MCLR/VPP", side:"right", top:38, gpio:3 },
    { name:"VDD", label:"5 VDD", side:"right", top:98 },
    { name:"GP2", label:"4 GP2/T0CKI/FOSC4", side:"right", top:158, gpio:2 }
  ],
  w25q: [
    { name:"/CS", label:"1 /CS", side:"left", top:12 },
    { name:"DO", label:"2 DO (MISO)", side:"left", top:42 },
    { name:"CLK", label:"6 CLK", side:"left", top:72 },
    { name:"DI", label:"5 DI (MOSI)", side:"left", top:102 }
  ],
  "i2c-lcd1602": [
    { name:"SDA", label:"SDA", side:"left", top:34 },
    { name:"SCL", label:"SCL", side:"left", top:84 }
  ],
  led: [{ name:"A", gpio:null }],
  pushbutton: [{ name:"1", gpio:null }],
  buzzer: [{ name:"1", gpio:null }]
};
pinDefs.pic10f202 = pinDefs.pic10f200;

function isMcuType(type) { return MCU_TYPES.has(type); }
function mcuDevice(part) {
  return part.type === "pic10f202" ? "PIC10F202" : "PIC10F200";
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

function partClass(type) {
  return isMcuType(type) ? "mcu"
    : type === "w25q" ? "flash-chip"
    : type === "i2c-lcd1602" ? "lcd1602-part"
    : type === "led" ? "led-part"
    : type === "pushbutton" ? "button-part" : "buzzer-part";
}

function pinKey(partId, pin) { return `${partId}:${pin}`; }
function snap(value) { return Math.round(value / SNAP_SIZE) * SNAP_SIZE; }
function stageX(worldX) { return worldX + ORIGIN_X; }
function stageY(worldY) { return worldY + ORIGIN_Y; }
function partSize(type) {
  if (isMcuType(type)) return { width: 300, height: 240 };
  if (type === "w25q") return { width: 180, height: 150 };
  if (type === "i2c-lcd1602") return { width: 430, height: 154 };
  if (type === "led") return { width: 48, height: 48 };
  if (type === "pushbutton") return { width: 110, height: 60 };
  return { width: 60, height: 60 };
}
function partTopLeft(part) {
  const size = partSize(part.type);
  if (isMcuType(part.type) || part.type === "w25q" ||
      part.type === "i2c-lcd1602") {
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
  const pinName = part.type === "led" ? "A" : "1";
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
      const otherSize = partSize(other.type);
      const otherCenter = otherPosition.left + otherSize.width / 2;
      return otherCenter < ownCenter ? "left" : "right";
    }
  }
  return part.type === "led" ? "right" : "left";
}

function devicePortYOffset(part) {
  return partSize(part.type).height / 2;
}

function renderPart(part) {
  const el = document.createElement("div");
  el.className = `part ${partClass(part.type)}`;
  el.dataset.id = part.id;
  const position = partTopLeft(part);
  el.style.left = `${stageX(position.left)}px`;
  el.style.top = `${stageY(position.top)}px`;
  if (part.type === "led") {
    el.classList.add(part.attrs?.color || "red");
  }

  if (isMcuType(part.type)) {
    el.innerHTML = `<div class="part-body">
      <i class="pin-one"></i><div class="mcu-title">${mcuDevice(part)}</div></div>`;
    for (const pin of pinDefs[part.type]) {
      const p = document.createElement("div");
      p.className = `pin ${pin.side}`;
      p.style.top = `${pin.top}px`;
      p.textContent = pin.label;
      p.dataset.pin = pin.name;
      p.addEventListener("pointerdown", event => {
        event.stopPropagation(); selectPin(part, pin.name, p);
      });
      el.querySelector(".part-body").appendChild(p);
    }
  } else if (part.type === "w25q") {
    el.innerHTML = `<div class="part-body">
      <i class="pin-one"></i><div class="flash-title">W25Q</div>
      <div class="flash-capacity-label"></div></div>
      <div class="part-label">${part.id}</div>`;
    for (const pinDef of pinDefs.w25q) {
      const pin = document.createElement("div");
      pin.className = "pin left";
      pin.style.top = `${pinDef.top}px`;
      pin.textContent = pinDef.label;
      pin.dataset.pin = pinDef.name;
      pin.addEventListener("pointerdown", event => {
        event.stopPropagation();
        selectPin(part, pinDef.name, pin);
      });
      el.querySelector(".part-body").appendChild(pin);
    }
    const capacity = Number(part.attrs?.capacity || 2097152);
    const capacityName = W25_CAPACITIES.find(item => item[0] === capacity);
    el.querySelector(".flash-capacity-label").textContent =
      capacityName ? capacityName[1].split(" · ")[0] : `${capacity} B`;
  } else if (part.type === "i2c-lcd1602") {
    el.innerHTML = `<div class="part-body">
      <div class="lcd-bezel"><div class="lcd-line" data-text="">${
        lcdLineHtml()}</div><div class="lcd-line" data-text="">${
        lcdLineHtml()}</div></div></div>
      <div class="part-label">${part.id} · 0x${Number(
        part.attrs?.address ?? 0x27).toString(16).toUpperCase()}</div>`;
    for (const pinDef of pinDefs["i2c-lcd1602"]) {
      const pin = document.createElement("div");
      pin.className = "pin left";
      pin.style.top = `${pinDef.top}px`;
      pin.textContent = pinDef.label;
      pin.dataset.pin = pinDef.name;
      pin.addEventListener("pointerdown", event => {
        event.stopPropagation();
        selectPin(part, pinDef.name, pin);
      });
      el.querySelector(".part-body").appendChild(pin);
    }
  } else if (part.type === "led") {
    el.innerHTML = `<div class="part-body"><i class="device-pin" data-pin="A"></i></div>
      <div class="part-label">${part.id}</div>`;
  } else if (part.type === "pushbutton") {
    el.innerHTML = `<div class="part-body"><span class="button-cap"></span>
      <i class="device-pin" data-pin="1"></i></div><div class="part-label">${part.id}</div>`;
  } else {
    el.innerHTML = `<div class="part-body">◉<i class="device-pin" data-pin="1"></i></div>
      <div class="part-label">${part.id}</div>`;
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
  if (isMcuType(part.type)) {
    const pin = pinDefs[part.type].find(item => item.name === pinName);
    if (!pin) return null;
    return {
      x: stageX(part.left) + (pin.side === "left" ? -6 : 306),
      y: stageY(part.top) + pin.top + 12,
      side: pin.side,
      top: stageY(part.top),
      bottom: stageY(part.top) + partSize(part.type).height
    };
  }
  if (part.type === "w25q" || part.type === "i2c-lcd1602") {
    const pin = pinDefs[part.type].find(item => item.name === pinName);
    if (!pin) return null;
    return {
      x: stageX(part.left) - 6,
      y: stageY(part.top) + pin.top + 12,
      side: "left",
      top: stageY(part.top),
      bottom: stageY(part.top) + partSize(part.type).height
    };
  }
  const size = partSize(part.type);
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
  configureAllW25(false).catch(error => message(error.message, true));
  configureAllLcd1602(false).catch(error => message(error.message, true));
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
  if (part.type === "pushbutton") {
    part.pressed = true;
    el.classList.add("pressed");
  }
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
    if (part.type === "pushbutton") {
      part.pressed = false;
      el.classList.remove("pressed");
    }
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
    if (isMcuType(part.type) || part.type === "w25q" ||
        part.type === "i2c-lcd1602") continue;
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
  const base = isMcuType(type) ? "mcu"
    : type === "w25q" ? "flash"
    : type === "i2c-lcd1602" ? "lcd"
    : type === "pushbutton" ? "button" : type;
  let index = 1, value = base;
  while (model.diagram.parts.some(part => part.id === value)) value = base + index++;
  return value;
}

document.querySelectorAll(".palette-item").forEach(item => {
  item.addEventListener("dragstart", event =>
    event.dataTransfer.setData("application/x-picemu-part", item.dataset.type));
});
stageViewport.addEventListener("dragover", event => event.preventDefault());
stageViewport.addEventListener("drop", event => {
  event.preventDefault();
  const type = event.dataTransfer.getData("application/x-picemu-part");
  if (!pinDefs[type]) return;
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
  if (type === "led") part.attrs = { color: "red" };
  if (type === "pushbutton") part.attrs = { activeLow: true };
  if (type === "w25q") part.attrs = { capacity: 2097152, data: "" };
  if (type === "i2c-lcd1602") part.attrs = { address: 0x27 };
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
        const size = partSize(part.type);
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
    const size = partSize(part.type);
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
  let mask = 0, values = 0;
  const mcu = model.diagram.parts.find(
    p => isMcuType(p.type) && p.id === mcuId);
  if (!mcu) return { inputMask: 0, inputValues: 0 };
  for (const part of model.diagram.parts.filter(p => p.type === "pushbutton")) {
    const connection = model.diagram.connections.find(c =>
      c.includes(`${part.id}:1`) &&
      c.some(value => value.startsWith(`${mcu.id}:GP`)));
    if (!connection) continue;
    const mcuEnd = connection.find(value => value.startsWith(`${mcu.id}:GP`));
    const pin = Number(mcuEnd.match(/GP(\d)/)?.[1]);
    if (!Number.isFinite(pin)) continue;
    mask |= 1 << pin;
    if (!part.pressed) values |= 1 << pin;
  }
  return { inputMask: mask, inputValues: values };
}

async function command(commandName, extra = {}, mcuId = model.activeMcuId) {
  const result = await post("/api/command", {
    command: commandName, mcuId, ...inputState(mcuId), ...extra
  });
  setState(result);
  return result;
}

function w25Wiring(part) {
  const pins = { "/CS": "csPin", CLK: "clockPin", DI: "mosiPin", DO: "misoPin" };
  const result = {};
  let mcuId = null;
  for (const [flashPin, property] of Object.entries(pins)) {
    const endpoint = `${part.id}:${flashPin}`;
    const connection = model.diagram.connections.find(item =>
      item[0] === endpoint || item[1] === endpoint);
    if (!connection) return null;
    const other = connection[0] === endpoint ? connection[1] : connection[0];
    const match = /^([^:]+):GP([0-3])$/.exec(other);
    if (!match || (mcuId !== null && mcuId !== match[1])) return null;
    mcuId = match[1];
    result[property] = Number(match[2]);
  }
  if (!mcuParts().some(part => part.id === mcuId)) return null;
  return { mcuId, ...result };
}

async function configureW25(part, strict = true) {
  const wiring = w25Wiring(part);
  if (!wiring) {
    if (strict) {
      throw new Error(`${part.id}需要把/CS、CLK、DI、DO连接到同一颗PIC`);
    }
    return false;
  }
  part.attrs ||= {};
  const result = await post("/api/command", {
    command: "w25_config",
    mcuId: wiring.mcuId,
    capacity: Number(part.attrs.capacity || 2097152),
    initialData: part.attrs.data || "",
    ...wiring
  });
  setState(result);
  return true;
}

async function configureAllW25(strict = true) {
  const flashes = model.diagram.parts.filter(part => part.type === "w25q");
  for (const flash of flashes) await configureW25(flash, strict);
}

function lcd1602Wiring(part) {
  const result = {};
  let mcuId = null;
  for (const [lcdPin, property] of [["SDA", "sdaPin"], ["SCL", "sclPin"]]) {
    const endpoint = `${part.id}:${lcdPin}`;
    const connection = model.diagram.connections.find(item =>
      item[0] === endpoint || item[1] === endpoint);
    if (!connection) return null;
    const other = connection[0] === endpoint ? connection[1] : connection[0];
    const match = /^([^:]+):GP([0-3])$/.exec(other);
    if (!match || (mcuId !== null && mcuId !== match[1])) return null;
    mcuId = match[1];
    result[property] = Number(match[2]);
  }
  return mcuParts().some(part => part.id === mcuId)
    ? { mcuId, ...result } : null;
}

async function configureLcd1602(part, strict = true) {
  const wiring = lcd1602Wiring(part);
  if (!wiring) {
    if (strict) throw new Error(`${part.id} must connect SDA and SCL to one PIC`);
    return false;
  }
  const result = await post("/api/command", {
    command: "lcd1602_config",
    mcuId: wiring.mcuId,
    address: Number(part.attrs?.address ?? 0x27),
    ...wiring
  });
  setState(result);
  return true;
}

async function configureAllLcd1602(strict = true) {
  const displays = model.diagram.parts.filter(
    part => part.type === "i2c-lcd1602");
  for (const display of displays) await configureLcd1602(display, strict);
}

function formatW25Dump(offset, data) {
  const rows = [];
  for (let index = 0; index < data.length; index += 16) {
    const bytes = data.slice(index, index + 16);
    const hexBytes = bytes.map(value =>
      value.toString(16).toUpperCase().padStart(2, "0")).join(" ");
    const ascii = bytes.map(value =>
      value >= 32 && value <= 126 ? String.fromCharCode(value) : ".").join("");
    rows.push(`${hex(offset + index, 6)}  ${hexBytes.padEnd(47)}  ${ascii}`);
  }
  return rows.join("\n");
}

async function refreshW25Memory(part) {
  if (!part || part.type !== "w25q" || model.w25RefreshPending) return;
  const wiring = w25Wiring(part);
  const dump = $("#w25Dump");
  if (!wiring || !dump) return;
  const capacity = Number(part.attrs?.capacity || 2097152);
  let offset = Number(model.w25Offsets.get(part.id) || 0);
  offset = Math.max(0, Math.min(capacity - 1, offset));
  offset = Math.floor(offset / 16) * 16;
  const count = Math.min(256, capacity - offset);
  model.w25RefreshPending = true;
  try {
    const result = await post("/api/command", {
      command: "w25_read", mcuId: wiring.mcuId, offset, count
    });
    model.w25Offsets.set(part.id, offset);
    dump.textContent = formatW25Dump(offset, result.data);
    const offsetInput = $("#w25Offset");
    if (offsetInput) offsetInput.value = String(offset);
    const pageText = $("#w25Page");
    if (pageText) {
      const totalPages = Math.ceil(capacity / 256);
      pageText.textContent =
        `第 ${Math.floor(offset / 256) + 1} / ${totalPages} 页`;
    }
  } catch (error) {
    dump.textContent = `读取失败：${error.message}`;
  } finally {
    model.w25RefreshPending = false;
  }
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

function connectedMcuPin(part) {
  const endpoint = `${part.id}:${part.type === "led" ? "A" : "1"}`;
  const connection = model.diagram.connections.find(c => c.includes(endpoint));
  if (!connection) return null;
  const other = connection.find(value => value !== endpoint && /:GP\d$/.test(value));
  if (!other) return null;
  const [mcuId, pinName] = other.split(":");
  return {
    mcuId,
    gpio: Number(pinName.match(/GP(\d)$/)[1]),
    state: model.states.get(mcuId)
  };
}

function updatePartsFromState() {
  if (model.states.size === 0) return;
  for (const part of model.diagram.parts) {
    const el = document.querySelector(`.part[data-id="${CSS.escape(part.id)}"]`);
    if (!el) continue;
    const connection = connectedMcuPin(part);
    if (part.type === "i2c-lcd1602") {
      const wiring = lcd1602Wiring(part);
      const lines = wiring
        ? model.states.get(wiring.mcuId)?.lcd1602?.lines : null;
      if (lines) {
        el.querySelectorAll(".lcd-line").forEach((line, index) => {
          const text = (lines[index] || "").padEnd(16).slice(0, 16);
          if (line.dataset.text !== text) {
            line.dataset.text = text;
            line.innerHTML = lcdLineHtml(text);
          }
        });
      }
    }
    if (part.type === "led" && connection?.state) {
      const { gpio, state } = connection;
      const duty = state.pinDuty?.[gpio] ?? ((state.gpio >> gpio) & 1);
      el.classList.toggle("lit", duty > 0.01);
      el.querySelector(".part-body").style.opacity =
        String(0.22 + Math.pow(duty, 0.55) * 0.78);
    }
  }
  for (const mcu of mcuParts()) {
    const state = model.states.get(mcu.id);
    if (!state) continue;
    document.querySelectorAll(
      `.part[data-id="${CSS.escape(mcu.id)}"] .pin[data-pin^='GP']`
    ).forEach(pin => {
      const number = Number(pin.dataset.pin.slice(2));
      const input = number === 3 ||
        (state.tris & (1 << number)) !== 0;
      const high = (state.gpio & (1 << number)) !== 0;
      pin.classList.toggle("gpio-input", input);
      pin.classList.toggle("gpio-output", !input);
      pin.classList.toggle("gpio-high", high);
      pin.classList.toggle("gpio-low", !high);
      pin.title = `${pin.dataset.pin}: ${input ? "输入" : "输出"}，电平 ${
        high ? "高" : "低"}`;
    });
  }
  updateBuzzer();
}

function updateBuzzer() {
  const buzzer = model.diagram.parts.find(p => p.type === "buzzer");
  const connection = buzzer ? connectedMcuPin(buzzer) : null;
  const frequency = connection?.state
    ? connection.state.pinFrequency?.[connection.gpio] || 0 : 0;
  if (frequency > 30 && frequency < 12000 && model.running) {
    if (!model.audio) {
      const WebAudioContext = window.AudioContext || window.webkitAudioContext;
      if (!WebAudioContext) return;
      model.audio = new WebAudioContext();
    }
    if (!model.oscillator) {
      model.oscillator = model.audio.createOscillator();
      model.gain = model.audio.createGain();
      model.gain.gain.value = 0.045;
      model.oscillator.connect(model.gain).connect(model.audio.destination);
      model.oscillator.start();
    }
    model.oscillator.frequency.setTargetAtTime(
      frequency, model.audio.currentTime, 0.01);
  } else if (model.oscillator) {
    model.oscillator.stop(); model.oscillator = null; model.gain = null;
  }
}

async function loadDiagram(diagram, name = "电路", diagramPath = null) {
  pause();
  model.diagram = diagram;
  model.diagramName = name;
  model.diagramPath = diagramPath;
  model.states.clear();
  model.state = null;
  model.activeMcuId = null;
  model.breakpoints.clear();
  model.w25Offsets.clear();
  model.selected = null;
  model.selectedIds.clear();
  model.selectedConnection = null;
  model.diagram.parts ||= [];
  model.diagram.connections ||= [];
  $("#diagramName").textContent = name;
  $("#saveDiagramBtn").disabled = diagramPath === null;
  renderAll();
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
    await configureAllW25(true);
    await configureAllLcd1602(true);
    const loaded = states.filter(Boolean).length;
    const flashCount =
      model.diagram.parts.filter(part => part.type === "w25q").length;
    message(`已加载 ${loaded} 个 MCU 固件${
      flashCount ? `和 ${flashCount} 个W25Q` : ""}`);
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
  updateBuzzer();
}

$("#runBtn").addEventListener("click", async () => {
  if (![...model.states.values()].some(state => state.loaded)) {
    return message("请先为至少一个芯片加载HEX固件", true);
  }
  model.running = true;
  model.lastFrame = performance.now();
  $("#runState").textContent = "运行中";
  $("#runState").className = "status running";
  if (model.audio?.state === "suspended") await model.audio.resume();
});
$("#pauseBtn").addEventListener("click", pause);
$("#stepBtn").addEventListener("click", async () => {
  pause();
  try {
    const before = model.state?.pc;
    const instruction = Number.isInteger(before)
      ? model.instructions[before] || "未知指令" : "未知指令";
    const result = await command("step");
    message(`单步 ${hex(before, 3)} ${instruction} → PC ${hex(result.pc, 3)}`);
  } catch (error) { message(error.message, true); }
});
$("#resetBtn").addEventListener("click", async () => {
  pause();
  try {
    await Promise.all([...model.states.entries()]
      .filter(([, state]) => state.loaded)
      .map(([id]) => command("reset", {}, id)));
  } catch (error) { message(error.message, true); }
});

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
  const selectedPart = model.diagram.parts.find(
    part => part.id === model.selected && part.type === "w25q");
  if (selectedPart && now - model.w25LastRefresh >= 200) {
    model.w25LastRefresh = now;
    refreshW25Memory(selectedPart);
  }
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

function showPartInspector(part) {
  const attrs = part.attrs || (part.attrs = {});
  const firstMcu = mcuParts()[0];
  const firmware = attrs.firmware ||
    (part === firstMcu ? model.diagram.firmware || "" : "");
  $("#partInspector").innerHTML = `
    <div class="property-row"><label>ID</label><input id="propId" value="${part.id}"></div>
    <div class="property-row"><label>类型</label><input value="${part.type}" disabled></div>
    ${part.type === "led" ? `<div class="property-row"><label>颜色</label>
      <select id="propColor"><option>red</option><option>green</option><option>blue</option><option>yellow</option></select></div>` : ""}
    ${isMcuType(part.type) ? `<div class="property-row"><label>HEX 固件</label>
      <input id="propFirmware" value="${firmware}" disabled>
      <button id="propHexBtn" type="button">为 ${part.id} 设置 HEX</button>
    </div>` : ""}
    ${part.type === "i2c-lcd1602" ? `
      <div class="property-row"><label>I²C address</label>
        <select id="propLcdAddress">
          <option value="39">0x27</option>
          <option value="63">0x3F</option>
        </select>
      </div>` : ""}
    ${part.type === "w25q" ? `
      <div class="property-row"><label>容量</label>
        <select id="propW25Capacity">${W25_CAPACITIES.map(([value, label]) =>
          `<option value="${value}">${label}</option>`).join("")}</select>
      </div>
      <div class="property-row">
        <label>写入地址0的数据（HEX）</label>
        <textarea id="propW25InitialData" name="w25-initial-${part.id}"
          rows="3" autocomplete="new-password"
          spellcheck="false" placeholder="例如：48 65 6C 6C 6F">${
          attrs.data || ""}</textarea>
        <button id="w25Write" type="button">写入</button>
      </div>
      <div class="property-row"><label>数据查看器</label>
        <div class="w25-page-row">
          <button id="w25Prev" type="button">上一页</button>
          <span id="w25Page">第 1 页</span>
          <button id="w25Next" type="button">下一页</button>
        </div>
        <div class="w25-jump-row">
          <input id="w25Offset" type="number" min="0" step="256" value="0">
          <button id="w25Go" type="button">跳转</button>
        </div>
        <pre id="w25Dump" class="w25-dump">等待读取……</pre>
      </div>` : ""}
    <div class="property-row"><label>位置</label><input id="propPosition" value="${part.left}, ${part.top}" disabled></div>`;
  $("#propId").addEventListener("change", async event => {
    const old = part.id, value = event.target.value.trim();
    if (!value || model.diagram.parts.some(p => p !== part && p.id === value)) {
      event.target.value = old; return;
    }
    part.id = value;
    if (part.type === "w25q") {
      const oldOffset = model.w25Offsets.get(old);
      model.w25Offsets.delete(old);
      if (oldOffset !== undefined) model.w25Offsets.set(value, oldOffset);
    }
    model.diagram.connections.forEach(c => {
      c[0] = c[0].replace(`${old}:`, `${value}:`);
      c[1] = c[1].replace(`${old}:`, `${value}:`);
    });
    if (isMcuType(part.type)) {
      const oldState = model.states.get(old);
      model.states.delete(old);
      if (oldState) model.states.set(value, { ...oldState, mcuId: value });
      const oldBreakpoints = model.breakpoints.get(old);
      model.breakpoints.delete(old);
      if (oldBreakpoints) model.breakpoints.set(value, oldBreakpoints);
      if (model.activeMcuId === old) model.activeMcuId = value;
      if (attrs.firmware) {
        try {
          setState(await post("/api/load", {
            mcuId: value,
            firmware: attrs.firmware,
            device: mcuDevice(part)
          }));
        } catch (error) {
          message(error.message, true);
        }
      }
    }
    model.selectedIds.delete(old);
    model.selectedIds.add(value);
    model.selected = value; renderAll();
    recordHistory();
  });
  const color = $("#propColor");
  if (color) {
    color.value = attrs.color || "red";
    color.addEventListener("change", e => {
      attrs.color = e.target.value;
      renderAll();
      recordHistory();
    });
  }
  const lcdAddress = $("#propLcdAddress");
  if (lcdAddress) {
    lcdAddress.value = String(attrs.address ?? 0x27);
    lcdAddress.addEventListener("change", async event => {
      attrs.address = Number(event.target.value);
      recordHistory();
      renderAll();
      try {
        await configureLcd1602(part, true);
      } catch (error) {
        message(error.message, true);
      }
    });
  }
  const hexButton = $("#propHexBtn");
  if (hexButton) {
    hexButton.addEventListener("click", () => {
      model.hexTargetId = part.id;
      selectActiveMcu(part.id);
      $("#hexInput").value = "";
      $("#hexInput").click();
    });
  }
  const capacitySelect = $("#propW25Capacity");
  if (capacitySelect) {
    $("#propW25InitialData").value = attrs.data || "";
    capacitySelect.value = String(attrs.capacity || 2097152);
    capacitySelect.addEventListener("change", async event => {
      attrs.capacity = Number(event.target.value);
      try {
        await configureW25(part, true);
        model.w25Offsets.set(part.id, 0);
        recordHistory();
        renderAll();
        showPartInspector(part);
        message(`${part.id}容量已更新，存储内容已按初始数据重新装载`);
      } catch (error) {
        message(error.message, true);
      }
    });
    $("#propW25InitialData").addEventListener("change", event => {
      attrs.data = event.target.value.trim();
      recordHistory();
    });
    const movePage = async delta => {
      const capacity = Number(attrs.capacity || 2097152);
      const current = Number(model.w25Offsets.get(part.id) || 0);
      model.w25Offsets.set(part.id,
        Math.max(0, Math.min(capacity - 1, current + delta)));
      await refreshW25Memory(part);
    };
    $("#w25Prev").addEventListener("click", () => movePage(-256));
    $("#w25Next").addEventListener("click", () => movePage(256));
    $("#w25Go").addEventListener("click", async () => {
      model.w25Offsets.set(part.id, Number($("#w25Offset").value || 0));
      await refreshW25Memory(part);
    });
    $("#w25Write").addEventListener("click", async () => {
      attrs.data = $("#propW25InitialData").value.trim();
      const wiring = w25Wiring(part);
      if (!wiring) return message(`${part.id}的SPI连线不完整`, true);
      try {
        await post("/api/command", {
          command: "w25_write",
          mcuId: wiring.mcuId,
          offset: 0,
          data: attrs.data
        });
        model.w25Offsets.set(part.id, 0);
        recordHistory();
        await refreshW25Memory(part);
        message(`${part.id}已从地址0写入输入数据`);
      } catch (error) {
        message(error.message, true);
      }
    });
    refreshW25Memory(part);
  }
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
    if (isMcuType(part.type)) {
      model.states.delete(part.id);
      model.breakpoints.delete(part.id);
    }
    if (part.type === "w25q") model.w25Offsets.delete(part.id);
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
