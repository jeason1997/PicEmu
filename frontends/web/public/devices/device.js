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
