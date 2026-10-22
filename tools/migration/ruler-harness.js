import {setupMusicRuler} from '/src/music-ruler.ts';

let time = 0, sequence = 0;
const callbacks = new Map();
Object.defineProperty(performance, 'now', {value: () => time});
window.requestAnimationFrame = (callback) => {
    callbacks.set(++sequence, callback);
    return sequence;
};
window.cancelAnimationFrame = (id) => callbacks.delete(id);
let ruler;
const host = document.getElementById('album-ticks');
window.resetRuler = (narrow, night) => {
    ruler?.destroy();
    time = 0;
    callbacks.clear();
    document.documentElement.style.setProperty('--ink', night ? '#e9eef2' : '#232722');
    document.documentElement.style.setProperty('--accent', night ? '#d9ad7d' : '#b56834');
    document.body.style.background = night ? '#08121f' : '#e8e5e1';
    host.style.setProperty('--tick-width', narrow ? '2px' : '3px');
    host.style.setProperty('--tick-gap', narrow ? '6px' : '8px');
    host.style.setProperty('--tick-rest', narrow ? '8px' : '10px');
    host.style.setProperty('--tick-height', narrow ? '21px' : '30px');
    host.style.setProperty('--tick-hover', narrow ? '16px' : '22px');
    ruler = setupMusicRuler(host);
};
window.rulerOperation = (op, fps, traceRequested) => {
    const trace = [];
    if (op.frames) {
        for (let f = 0; f < op.frames; ++f) {
            time += 1000 / fps;
            const work = [...callbacks.values()];
            callbacks.clear();
            for (const callback of work) {
                callback(time);
            }
            if (traceRequested) {
                trace.push(ruler.probe());
            }
        }
    } else {
        const items =
            Array.from({length: op.count}, (_, i) => ({index: i, id: 'a' + i, title: '专辑 ' + i}));
        ruler.update(items, op.row, false,
                     op.direction ? {axis: 'row', direction: op.direction} : undefined, op.loop);
    }
    return {...ruler.probe(), trace};
};
window.ready = true;
