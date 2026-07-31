/*
 * Web 器件定义的公共辅助函数。
 *
 * 器件模块只描述自身的外观、尺寸和引脚，不直接读取 app.js 的全局状态。
 * 这种约束让同一器件定义以后可以复用于缩略图、只读预览等其他 Web 视图。
 */
export function pin(name, label, side, top, gpio = undefined) {
  return { name, label, side, top, gpio };
}

export function labeledPart(body, id, extraLabel = "") {
  return `${body}<div class="part-label">${id}${extraLabel}</div>`;
}

/*
 * 按给定引脚映射解析“本器件的若干引脚连接到同一颗 PIC GPIO”。协议器件
 * 共用这段校验，具体器件模块仍负责决定需要哪些引脚以及后端字段名称。
 */
export function picWiring(context, part, pinMap) {
  const result = {};
  let mcuId = null;
  for (const [devicePin, property] of Object.entries(pinMap)) {
    const endpoint = `${part.id}:${devicePin}`;
    const connection = context.model.diagram.connections.find(item =>
      item[0] === endpoint || item[1] === endpoint);
    if (!connection) return null;
    const other = connection[0] === endpoint ? connection[1] : connection[0];
    const match = /^([^:]+):GP([0-3])$/.exec(other);
    if (!match || (mcuId !== null && mcuId !== match[1])) return null;
    mcuId = match[1];
    result[property] = Number(match[2]);
  }
  return context.mcuParts().some(mcu => mcu.id === mcuId)
    ? { mcuId, ...result } : null;
}
