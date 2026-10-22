import {setupMusicTitleLayout} from '/src/music-title.ts';
const stage = document.querySelector('#stage');
const initialMarkup = stage.innerHTML;
let callout;
const style = document.createElement('style');
style.textContent = `
html,body,#stage { background: var(--page-bg, #e8e5e1); }
.music-header,.music-navigation,.music-bottomline,.music-vignette,.music-browse-veil,
#music-loading,#library-status,.music-view-toggle { visibility:hidden!important; }
#stage .album-callout { position:absolute;left:0;top:0;right:auto;padding:0;max-height:none;min-height:0;max-width:none;overflow:visible; }
.album-callout > :not(h1) {display:none!important}
#stage .album-callout h1 { margin:0; }
#stage .album-callout h1.music-title-probe { font-size:var(--base-title-size); }
`;
document.head.append(style);
let title;
window.renderCase = async (input) => {
    title?.destroy();
    stage.innerHTML = initialMarkup;
    callout = stage.querySelector('.album-callout');
    stage.dataset.theme = input.night ? 'night' : 'day';
    stage.dataset.layout = 'desktop';
    stage.classList.add('reduce-motion');
    document.body.style.background = input.night ? '#08121f' : '#e8e5e1';
    callout.style.width = input.width + 'px';
    callout.style.height = input.budget + 'px';
    callout.style.setProperty('--base-title-size', input.size + 'px');
    title = setupMusicTitleLayout(stage);
    window.lastTitleLayout = null;
    title.update(input.text, false);
    await document.fonts.ready;
    // ResizeObserver and the source's own queued layout must settle after changing the probe.
    for (let frame = 0; frame < 12; ++frame)
        await new Promise((resolve) => requestAnimationFrame(resolve));
    return window.lastTitleLayout;
};
window.sampleTracks = () => {
    const result = [];
    for (const fps of [20, 30, 60, 120]) {
        const element = document.createElement('span');
        document.body.append(element);
        const track =
            new window.SourceTitleTrack(element, 'transform', ([x]) => `translateY(${x}px)`);
        track.set([0]);
        let elapsed = 0;
        for (const op of [{target: 60}, {frames: Math.floor(fps * .2)}, {target: -30},
                          {frames: Math.floor(fps * .1)}, {target: 90}, {frames: fps}, {target: 90},
                          {target: 0}, {frames: fps}]) {
            if (op.target !== undefined) {
                track.move([op.target]);
                elapsed = 0;
                track.animation?.pause();
                if (track.animation)
                    track.animation.currentTime = 0;
                result.push({fps, op, sample: track.sample()});
            } else {
                for (let i = 0; i < op.frames; ++i) {
                    elapsed += 1000 / fps;
                    if (track.animation)
                        track.animation.currentTime = elapsed;
                    result.push({fps, op: {dt: 1 / fps}, sample: track.sample()});
                }
            }
        }
        track.cancel();
        element.remove();
    }
    return result;
};
await Promise.all(
    [300, 400, 600, 700].map((weight) => document.fonts.load(weight + ' 44px MiSans')));
await document.fonts.ready;
window.ready = true;
