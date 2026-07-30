const $ = selector => document.querySelector(selector);
const stage = $("#stage");
const stageSizer = $("#stageSizer");
const stageViewport = $("#stageViewport");
const partsLayer = $("#partsLayer");
const wires = $("#wires");
const STAGE_WIDTH = 1400;
const STAGE_HEIGHT = 900;
const GRID_SIZE = 20;
const SNAP_SIZE = GRID_SIZE / 2;

const model = {
  diagram: { version: 1, clockHz: 4000000, firmware: "", parts: [], connections: [] },
  state: null,
  flash: [],
  instructions: [],
  running: false,
  selected: null,
  selectedConnection: null,
  pendingPin: null,
  lastFrame: performance.now(),
  budget: 0,
  requestPending: false,
  audio: null,
  oscillator: null,
  gain: null,
  zoom: 1
};

const pinDefs = {
  pic10f200: [
    { name:"GP0", label:"1 GP0/ICSPDAT", side:"left", top:44, gpio:0 },
    { name:"VSS", label:"2 VSS", side:"left", top:102 },
    { name:"GP1", label:"3 GP1/ICSPCLK", side:"left", top:160, gpio:1 },
    { name:"GP3", label:"6 GP3/MCLR/VPP", side:"right", top:44, gpio:3 },
    { name:"VDD", label:"5 VDD", side:"right", top:102 },
    { name:"GP2", label:"4 GP2/T0CKI/FOSC4", side:"right", top:160, gpio:2 }
  ],
  led: [{ name:"A", gpio:null }],
  pushbutton: [{ name:"1", gpio:null }],
  buzzer: [{ name:"1", gpio:null }]
};

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
  return type === "pic10f200" ? "mcu"
    : type === "led" ? "led-part"
    : type === "pushbutton" ? "button-part" : "buzzer-part";
}

function pinKey(partId, pin) { return `${partId}:${pin}`; }
function snap(value) { return Math.round(value / SNAP_SIZE) * SNAP_SIZE; }

function devicePortSide(part) {
  const endpoint = `${part.id}:${part.type === "led" ? "A" : "1"}`;
  const connection = model.diagram.connections.find(c => c.includes(endpoint));
  let target = null;
  if (connection) {
    const other = connection.find(value => value !== endpoint);
    const targetId = other?.split(":")[0];
    target = model.diagram.parts.find(item => item.id === targetId);
  }
  target ||= model.diagram.parts.find(item => item.type === "pic10f200");
  if (!target) return "right";
  const targetCenter = target.left +
    (target.type === "pic10f200" ? 130 : 31);
  const partCenter = part.left + (part.type === "led" ? 27 : 31);
  return targetCenter < partCenter ? "left" : "right";
}

function renderPart(part) {
  const el = document.createElement("div");
  el.className = `part ${partClass(part.type)}`;
  el.dataset.id = part.id;
  el.style.left = `${part.left}px`;
  el.style.top = `${part.top}px`;
  if (part.type === "led") {
    el.classList.add(part.attrs?.color || "red");
  }

  if (part.type === "pic10f200") {
    el.innerHTML = `<div class="part-body">
      <i class="pin-one"></i><div class="mcu-title">PIC10F200</div></div>`;
    for (const pin of pinDefs.pic10f200) {
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
  if (part.type === "pic10f200") {
    const pin = pinDefs.pic10f200.find(item => item.name === pinName);
    if (!pin) return null;
    return {
      x: part.left + (pin.side === "left" ? -12 : 272),
      y: part.top + pin.top + 12
    };
  }
  const width = part.type === "led" ? 54 : 62;
  const side = devicePortSide(part);
  return {
    x: part.left + (side === "left" ? -7 : width + 7),
    y: part.top + 26
  };
}

function renderWires() {
  wires.innerHTML = "";
  model.diagram.connections.forEach((connection, index) => {
    const a = pinPoint(connection[0]);
    const b = pinPoint(connection[1]);
    if (!a || !b) return;
    const middle = snap((a.x + b.x) / 2);
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("class", "wire");
    if (index === model.selectedConnection) path.classList.add("selected");
    path.setAttribute("stroke", connection[2] || "#60a5fa");
    path.setAttribute("d", `M${a.x},${a.y} H${middle} V${b.y} H${b.x}`);
    path.addEventListener("pointerdown", event => {
      event.stopPropagation();
      model.selected = null;
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
    }
  }
  model.pendingPin = null;
  renderPortDirections();
  renderWires();
  message("连线已更新");
}

function beginPartDrag(event, part, el) {
  if (event.button !== 0 || event.target.matches(".pin,.device-pin")) return;
  event.preventDefault();
  model.selected = part.id; updateSelection(); showPartInspector(part);
  model.selectedConnection = null;
  const startX = event.clientX, startY = event.clientY;
  const left = part.left, top = part.top;
  if (part.type === "pushbutton") {
    part.pressed = true;
    el.classList.add("pressed");
  }
  el.setPointerCapture(event.pointerId);
  const move = e => {
    part.left = snap(left + (e.clientX - startX) / model.zoom);
    part.top = snap(top + (e.clientY - startY) / model.zoom);
    el.style.left = `${part.left}px`; el.style.top = `${part.top}px`;
    renderPortDirections();
    renderWires();
  };
  const up = () => {
    if (part.type === "pushbutton") {
      part.pressed = false;
      el.classList.remove("pressed");
    }
    el.removeEventListener("pointermove", move);
    el.removeEventListener("pointerup", up);
    el.removeEventListener("pointercancel", up);
  };
  el.addEventListener("pointermove", move);
  el.addEventListener("pointerup", up);
  el.addEventListener("pointercancel", up);
}

function renderPortDirections() {
  for (const part of model.diagram.parts) {
    if (part.type === "pic10f200") continue;
    const pin = document.querySelector(
      `.part[data-id="${CSS.escape(part.id)}"] .device-pin`);
    if (!pin) continue;
    pin.classList.remove("port-left", "port-right");
    pin.classList.add(`port-${devicePortSide(part)}`);
  }
}

function updateSelection() {
  document.querySelectorAll(".part").forEach(el =>
    el.classList.toggle("selected", el.dataset.id === model.selected));
}

function uniqueId(type) {
  const base = type === "pic10f200" ? "mcu"
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
    left: snap(Math.max(20,
      (stageViewport.scrollLeft + event.clientX - rect.left) / model.zoom)),
    top: snap(Math.max(20,
      (stageViewport.scrollTop + event.clientY - rect.top) / model.zoom))
  };
  if (type === "led") part.attrs = { color: "red" };
  if (type === "pushbutton") part.attrs = { activeLow: true };
  model.diagram.parts.push(part);
  model.selected = part.id; renderAll(); showPartInspector(part);
});

stageViewport.addEventListener("pointerdown", event => {
  if (event.button !== 0 ||
      event.target.closest?.(".part") ||
      event.target.closest?.(".wire")) return;
  model.selected = null;
  model.selectedConnection = null;
  if (model.pendingPin) {
    model.pendingPin.element.classList.remove("connecting");
    model.pendingPin = null;
  }
  updateSelection();
  renderWires();
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
    const width = part.type === "pic10f200" ? 260
      : part.type === "led" ? 54 : 62;
    const height = part.type === "pic10f200" ? 206
      : part.type === "led" ? 80 : 72;
    minX = Math.min(minX, part.left);
    minY = Math.min(minY, part.top);
    maxX = Math.max(maxX, part.left + width);
    maxY = Math.max(maxY, part.top + height);
  }
  const margin = 80;
  const zoom = Math.min(1,
    (stageViewport.clientWidth - margin) / (maxX - minX),
    (stageViewport.clientHeight - margin) / (maxY - minY));
  setZoom(Math.max(0.35, zoom));
  stageViewport.scrollLeft =
    ((minX + maxX) / 2) * model.zoom - stageViewport.clientWidth / 2;
  stageViewport.scrollTop =
    ((minY + maxY) / 2) * model.zoom - stageViewport.clientHeight / 2;
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

function inputState() {
  let mask = 0, values = 0;
  const mcu = model.diagram.parts.find(p => p.type === "pic10f200");
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

async function command(commandName, extra = {}) {
  const result = await post("/api/command", {
    command: commandName, ...inputState(), ...extra
  });
  setState(result);
  return result;
}

function setState(state) {
  model.state = state;
  if (state.flash) model.flash = state.flash;
  if (state.instructions) model.instructions = state.instructions;
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
}

function connectedGpio(part) {
  const endpoint = `${part.id}:${part.type === "led" ? "A" : "1"}`;
  const connection = model.diagram.connections.find(c => c.includes(endpoint));
  if (!connection) return null;
  const other = connection.find(value => value !== endpoint && /:GP\d$/.test(value));
  return other ? Number(other.match(/GP(\d)$/)[1]) : null;
}

function updatePartsFromState() {
  if (!model.state) return;
  for (const part of model.diagram.parts) {
    const el = document.querySelector(`.part[data-id="${CSS.escape(part.id)}"]`);
    if (!el) continue;
    const gpio = connectedGpio(part);
    if (part.type === "led" && gpio !== null) {
      const duty = model.state.pinDuty?.[gpio] ??
        ((model.state.gpio >> gpio) & 1);
      el.classList.toggle("lit", duty > 0.01);
      el.querySelector(".part-body").style.opacity =
        String(0.22 + Math.pow(duty, 0.55) * 0.78);
    }
  }
  updateBuzzer();
}

function updateBuzzer() {
  const buzzer = model.diagram.parts.find(p => p.type === "buzzer");
  const gpio = buzzer ? connectedGpio(buzzer) : null;
  const frequency = gpio === null ? 0 : model.state?.pinFrequency?.[gpio] || 0;
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

async function loadDiagram(diagram, name = "电路") {
  pause();
  model.diagram = diagram;
  model.diagram.parts ||= [];
  model.diagram.connections ||= [];
  $("#diagramName").textContent = name;
  renderAll();
  requestAnimationFrame(fitDiagram);
  const mcu = diagram.parts.find(p => p.type === "pic10f200");
  if (!mcu || !diagram.firmware) {
    message("电路已载入；请设置HEX固件");
    return;
  }
  try {
    const state = await post("/api/load", {
      firmware: diagram.firmware, device: "PIC10F200"
    });
    setState(state);
    message(`已加载 ${diagram.firmware}`);
  } catch (error) {
    message(`固件加载失败：${error.message}`, true);
  }
}

async function openExample() {
  const path = $("#exampleSelect").value;
  if (!path) return;
  const diagram = await api(`/api/file?path=${encodeURIComponent(path)}`);
  await loadDiagram(diagram, path.split("/")[1]);
}

function pause() {
  model.running = false;
  $("#runState").textContent = "已暂停";
  $("#runState").className = "status paused";
  updateBuzzer();
}

$("#runBtn").addEventListener("click", async () => {
  if (!model.state?.loaded) return message("请先加载HEX固件", true);
  model.running = true;
  model.lastFrame = performance.now();
  $("#runState").textContent = "运行中";
  $("#runState").className = "status running";
  if (model.audio?.state === "suspended") await model.audio.resume();
});
$("#pauseBtn").addEventListener("click", pause);
$("#stepBtn").addEventListener("click", async () => {
  pause();
  try { await command("step"); } catch (error) { message(error.message, true); }
});
$("#resetBtn").addEventListener("click", async () => {
  pause();
  try { await command("reset"); } catch (error) { message(error.message, true); }
});

async function frame(now) {
  const elapsed = Math.min(0.1, (now - model.lastFrame) / 1000);
  model.lastFrame = now;
  if (model.running && model.state?.loaded && !model.requestPending) {
    const speed = Number($("#speedSelect").value);
    const instructionHz = Number(model.diagram.clockHz || 4000000) / 4;
    model.budget += elapsed * instructionHz * speed;
    const cycles = Math.min(100000, Math.floor(model.budget));
    if (cycles > 0) {
      model.budget -= cycles; model.requestPending = true;
      try {
        await command("run", { cycles });
        if (model.state.stopped) {
          pause(); message(model.state.stopReason || "CPU已停止", true);
        }
      } catch (error) {
        pause(); message(error.message, true);
      } finally {
        model.requestPending = false;
      }
    }
  }
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

function showPartInspector(part) {
  document.querySelector('[data-tab="part"]').click();
  const attrs = part.attrs || (part.attrs = {});
  $("#partInspector").innerHTML = `
    <div class="property-row"><label>ID</label><input id="propId" value="${part.id}"></div>
    <div class="property-row"><label>类型</label><input value="${part.type}" disabled></div>
    ${part.type === "led" ? `<div class="property-row"><label>颜色</label>
      <select id="propColor"><option>red</option><option>green</option><option>blue</option><option>yellow</option></select></div>` : ""}
    <div class="property-row"><label>位置</label><input value="${part.left}, ${part.top}" disabled></div>`;
  $("#propId").addEventListener("change", event => {
    const old = part.id, value = event.target.value.trim();
    if (!value || model.diagram.parts.some(p => p !== part && p.id === value)) {
      event.target.value = old; return;
    }
    part.id = value;
    model.diagram.connections.forEach(c => {
      c[0] = c[0].replace(`${old}:`, `${value}:`);
      c[1] = c[1].replace(`${old}:`, `${value}:`);
    });
    model.selected = value; renderAll();
  });
  const color = $("#propColor");
  if (color) {
    color.value = attrs.color || "red";
    color.addEventListener("change", e => { attrs.color = e.target.value; renderAll(); });
  }
}

document.querySelectorAll(".tab").forEach(tab => tab.addEventListener("click", () => {
  document.querySelectorAll(".tab,.tab-panel").forEach(item => item.classList.remove("active"));
  tab.classList.add("active");
  $(`#${tab.dataset.tab}Panel`).classList.add("active");
}));

$("#deleteBtn").addEventListener("click", () => {
  if (model.selectedConnection !== null) {
    model.diagram.connections.splice(model.selectedConnection, 1);
    model.selectedConnection = null;
    renderPortDirections();
    renderWires();
    message("已删除连线");
    return;
  }
  if (!model.selected) return;
  const prefix = model.selected + ":";
  model.diagram.parts = model.diagram.parts.filter(p => p.id !== model.selected);
  model.diagram.connections = model.diagram.connections.filter(c =>
    !c[0].startsWith(prefix) && !c[1].startsWith(prefix));
  model.selected = null; renderAll();
});
window.addEventListener("keydown", event => {
  if (event.target.matches("input,select,textarea")) return;
  if (event.key === "Delete" || event.key === "Backspace") {
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
$("#hexBtn").addEventListener("click", () => $("#hexInput").click());
$("#hexInput").addEventListener("change", async event => {
  const file = event.target.files[0];
  if (!file) return;
  try {
    const state = await post("/api/upload-firmware", {
      text: await file.text(), device: "PIC10F200"
    });
    model.diagram.firmware = state.firmware;
    setState(state); message(`已加载本地固件 ${file.name}`);
  } catch (error) { message(error.message, true); }
});
$("#exportBtn").addEventListener("click", () => {
  const blob = new Blob([JSON.stringify(model.diagram, null, 2)], {type:"application/json"});
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob); link.download = "diagram.json"; link.click();
  URL.revokeObjectURL(link.href);
});
$("#partSearch").addEventListener("input", event => {
  const query = event.target.value.toLowerCase();
  document.querySelectorAll(".palette-item").forEach(item =>
    item.style.display = item.textContent.toLowerCase().includes(query) ? "" : "none");
});

async function init() {
  const result = await api("/api/examples");
  $("#exampleSelect").innerHTML = result.examples.map(example =>
    `<option value="${example.diagram}" ${example.name === result.initialExample ? "selected" : ""}>${example.name}</option>`).join("");
  await openExample();
}
init().catch(error => message(error.message, true));
