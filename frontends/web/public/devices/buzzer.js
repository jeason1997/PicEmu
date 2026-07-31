import { labeledPart } from "./device.js";

let audio = null;
let oscillator = null;
let gain = null;

export default {
  type: "buzzer",
  category: "输出器件",
  categoryOrder: 1,
  palette: {
    title: "蜂鸣器", detail: "无源蜂鸣器",
    iconClass: "buzzer-icon", iconText: "◉", order: 1
  },
  className: "buzzer-part",
  size: { width: 60, height: 60 },
  pins: [{ name: "1", dynamic: true }],
  defaultPortSide: "left",
  styles: `
    .buzzer-icon {
      border-radius:50%;border:2px solid #94a3b8;font-size:18px;
    }
    .buzzer-part .part-body {
      width:60px;height:60px;border-radius:50%;background:#242d39;
      border:3px solid #94a3b8;display:grid;place-items:center;font-size:22px;
    }`,
  render(part) {
    return labeledPart(
      `<div class="part-body">◉<i class="device-pin" data-pin="1"></i></div>`,
      part.id
    );
  },
  update(context, part) {
    const connection = context.connectedMcuPin(part, "1");
    const frequency = connection?.state
      ? connection.state.pinFrequency?.[connection.gpio] || 0 : 0;
    if (frequency > 30 && frequency < 12000 && context.model.running) {
      if (!audio) {
        const AudioContextType = window.AudioContext || window.webkitAudioContext;
        if (!AudioContextType) return;
        audio = new AudioContextType();
      }
      if (!oscillator) {
        oscillator = audio.createOscillator();
        gain = audio.createGain();
        gain.gain.value = 0.045;
        oscillator.connect(gain).connect(audio.destination);
        oscillator.start();
      }
      oscillator.frequency.setTargetAtTime(frequency, audio.currentTime, 0.01);
    } else if (oscillator) {
      oscillator.stop();
      oscillator = null;
      gain = null;
    }
  },
  diagramUnload() {
    if (oscillator) oscillator.stop();
    oscillator = null;
    gain = null;
  }
};
