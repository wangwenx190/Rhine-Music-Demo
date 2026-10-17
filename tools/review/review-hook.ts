// Injected only into the detached review copy. Production commands/math remain frozen.
let reviewTime = 0;
const reviewAnimations = new Map < Animation, {
    start: number;
    current: number
}
>();
const reviewAnimate = Element.prototype.animate;
Element.prototype.animate = function(...args: Parameters<Element['animate']>) {
    const animation = reviewAnimate.apply(this, args);
    animation.pause();
    animation.currentTime = 0;
    reviewAnimations.set(animation, {start: reviewTime, current: 0});
    return animation;
};
(window as any).__baseline = {
    get ready() {
        return ready && scene?.getStats().loaded;
    },
    get scene() {
        return scene;
    },
    get state() {
        return {
            time: reviewTime,
            phase: presentation.phase,
            selected: currentAlbum()?.id,
            opening: boot?.root.dataset.appTime,
            arrayMode: preferences.arrayMode,
            theme: preferences.theme
        };
    },
    async pause() {
        frameSuspended = true;
        cancelAnimationFrame(frameHandle);
        frameHandle = 0;
        reviewTime = 0;
        textMotionLastFrame = 0;
        textMotionTime = 0;
        if (!scene)
            throw new Error('Scene not ready');
        (scene as any).last = (scene as any).clock = 0;
        const original = scene.update.bind(scene);
        scene.update = (time, cinematic) => original(time, cinematic, false);
        for (const animation of document.getAnimations()) {
            animation.pause();
            reviewAnimations.set(animation,
                                 {start: 0, current: Number(animation.currentTime) || 0});
        }
        await document.fonts.ready;
    },
    command(event: any) {
        switch (event.type) {
        case 'open':
            setMode('detail');
            break;
        case 'back':
            setMode('archive');
            break;
        case 'select':
            select(event.index, undefined, event.openAfter ?? presentation.openingOrDetail,
                   event.route);
            break;
        case 'overview':
            setOverview(event.active);
            break;
        case 'theme':
            setTheme(event.value);
            break;
        case 'speed':
            setMusicMotionSpeed(event.value);
            break;
        case 'opening':
            boot?.start(reviewTime);
            break;
        case 'skip':
            boot?.skip();
            break;
        case 'reduced':
            preferences.reduced = event.value;
            scene?.setReduced(event.value);
            break;
        default:
            throw new Error('Unknown review event ' + event.type);
        }
    },
    async advance(seconds: number) {
        const target = reviewTime + seconds;
        while (reviewTime < target - 1e-10) {
            reviewTime += Math.min(1 / 120, target - reviewTime);
            for (const [animation, saved] of reviewAnimations) {
                if (animation.playState === 'idle' || animation.playState === 'finished') {
                    reviewAnimations.delete(animation);
                    continue;
                }
                const elapsed =
                    saved.current + (reviewTime - saved.start) * 1000 * animation.playbackRate;
                const end = Number(animation.effect?.getComputedTiming().endTime);
                animation.currentTime = elapsed;
                if (elapsed >= end && Number.isFinite(end))
                    animation.finish();
            }
            await Promise.resolve();
            await Promise.resolve();
            frameSuspended = false;
            frame(reviewTime * 1000);
            cancelAnimationFrame(frameHandle);
            frameHandle = 0;
            frameSuspended = true;
        }
        await new Promise(resolve => setTimeout(resolve, 0));
        return this.state;
    },
    async render() {
        if (!scene)
            throw new Error('Scene not ready');
        await (scene as any).covers?.prepareVisible(1200);
        (scene as any).covers?.flushUploads(scene.renderer, 64);
        (scene as any).composer.render();
        return this.state;
    },
};
