import { labeledPart, pin } from "./device.js";
import { wiring as hc595Wiring } from "./hc595.js";

const segmentNames = ["a", "b", "c", "d", "e", "f", "g", "dp"];

function wiring(context, part) {
  let chipId = null;
  for (let index = 0; index < segmentNames.length; index++) {
    const endpoint = `${part.id}:${segmentNames[index]}`;
    const connection = context.model.diagram.connections.find(item =>
      item[0] === endpoint || item[1] === endpoint);
    if (!connection) return null;
    const other = connection[0] === endpoint ? connection[1] : connection[0];
    const match = /^([^:]+):Q([0-7])$/.exec(other);
    if (!match || Number(match[2]) !== index ||
        (chipId !== null && chipId !== match[1])) return null;
    chipId = match[1];
  }
  const chip = context.model.diagram.parts.find(item =>
    item.id === chipId && item.type === "hc595");
  const chipWiring = chip ? hc595Wiring(context, chip) : null;
  return chipWiring ? { chipId, mcuId: chipWiring.mcuId } : null;
}

async function configure(context, part, strict = true) {
  const connection = wiring(context, part);
  if (!connection) {
    if (strict) {
      throw new Error(`${part.id} 的 a～dp 必须依次连接到同一颗 74HC595 的 Q0～Q7`);
    }
    return false;
  }
  context.setState(await context.post("/api/command", {
    command: "seven_segment_config", mcuId: connection.mcuId,
    activeHigh: part.attrs?.activeHigh !== false
  }));
  return true;
}

export default {
  type: "seven-segment",
  defaultAttrs: { activeHigh: true, color: "red" },
  category: "输出器件",
  categoryOrder: 1,
  palette: {
    title: "七段数码管", detail: "a～g、dp 独立输入",
    iconClass: "seven-segment-icon", iconText: "8.", order: 3
  },
  className: "seven-segment-part",
  size: { width: 150, height: 190 },
  positionMode: "top-left",
  styles: `
    .seven-segment-icon {
      background:#26090c;color:#ff5360;font:700 22px ui-monospace,monospace;
      border:2px solid #7d2930;border-radius:5px;
    }
    .seven-segment-part .part-body {
      width:150px;height:190px;background:#241013;border:3px solid #6e2930;
      border-radius:10px;box-shadow:inset 0 0 22px #000;
    }
    .seven-display {
      position:absolute;left:48px;top:19px;width:78px;height:145px;
    }
    .seven-display i {
      position:absolute;background:#54141a;border-radius:6px;
      transition:background .04s,box-shadow .04s;
    }
    .seven-display i.on {
      background:#ff3545;box-shadow:0 0 12px #ff3545;
    }
    .seven-display [data-segment="a"],
    .seven-display [data-segment="d"],
    .seven-display [data-segment="g"] {
      width:54px;height:10px;left:4px;
    }
    .seven-display [data-segment="a"] { top:0; }
    .seven-display [data-segment="g"] { top:62px; }
    .seven-display [data-segment="d"] { top:124px; }
    .seven-display [data-segment="b"],
    .seven-display [data-segment="c"],
    .seven-display [data-segment="e"],
    .seven-display [data-segment="f"] { width:10px;height:54px; }
    .seven-display [data-segment="f"] { left:0;top:7px; }
    .seven-display [data-segment="b"] { left:56px;top:7px; }
    .seven-display [data-segment="e"] { left:0;top:70px; }
    .seven-display [data-segment="c"] { left:56px;top:70px; }
    .seven-display [data-segment="dp"] {
      width:11px;height:11px;left:68px;top:123px;border-radius:50%;
    }`,
  pins: ["a","b","c","d","e","f","g","dp"].map(
    (name, index) => pin(name, name, "left", 9 + index * 22)
  ),
  render(part) {
    return labeledPart(`<div class="part-body"><div class="seven-display">
      ${["a","b","c","d","e","f","g","dp"].map(
        segment => `<i data-segment="${segment}"></i>`).join("")}
      </div></div>`, part.id);
  },
  configure,
  update(context, part, element) {
    const connection = wiring(context, part);
    const segments = connection
      ? context.model.states.get(connection.mcuId)?.sevenSegment?.segments : null;
    if (segments === null || segments === undefined) return;
    segmentNames.forEach((name, bit) =>
      element.querySelector(`[data-segment="${name}"]`)
        ?.classList.toggle("on", (segments & (1 << bit)) !== 0));
  }
};
