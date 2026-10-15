import {setupMusicTitleLayout} from '/src/music-title.ts';

const stage = document.querySelector('#stage');
for (const element of document.querySelectorAll(
         '#music-loading,#library-status,.music-view-toggle,.music-navigation,.music-keyhint')) {
    element.style.visibility = 'hidden';
}
document.querySelector('#album-ticks').className = 'music-ruler';
document.querySelector('#selection-genre').textContent = '音乐';
document.querySelector('#selection-artist').textContent = '本地音乐收藏';
document.querySelector('#selection-meta').textContent = '2026  ·  12 TRACKS';
document.querySelector('#library-count').textContent = '27 ALBUMS';
document.querySelector('#runtime-info').textContent = 'QT / LOCAL';
let title = setupMusicTitleLayout(stage);

const paint = () => new Promise((resolve) => {
    requestAnimationFrame(() => requestAnimationFrame(resolve));
});
const bounds = (selector) => {
    const element = document.querySelector(selector);
    const rect = element.getBoundingClientRect();
    const style = getComputedStyle(element);
    const context = document.createElement('canvas').getContext('2d');
    context.font = style.font;
    const font = context.measureText('音乐');
    return {
        x: rect.x,
        y: rect.y,
        width: rect.width,
        height: rect.height,
        fontSize: style.fontSize,
        lineHeight: style.lineHeight,
        fontWeight: style.fontWeight,
        paddingTop: style.paddingTop,
        paddingBottom: style.paddingBottom,
        borderBottom: style.borderBottomWidth,
        fontAscent: font.fontBoundingBoxAscent,
        fontDescent: font.fontBoundingBoxDescent,
    };
};
window.renderCase = async (input) => {
    stage.dataset.layout = input.width / input.height < 1.05 ? 'portrait' : 'desktop';
    stage.dataset.theme = input.night ? 'night' : 'day';
    for (const button of stage.querySelectorAll('.theme-switch button')) {
        button.setAttribute('aria-pressed', String(button.dataset.theme === stage.dataset.theme));
    }
    // Each static fixture starts with a fresh source layout, rather than a resize's interim reels.
    title.destroy();
    title = setupMusicTitleLayout(stage);
    title.update('收藏的声音', false);
    await document.fonts.ready;
    await new Promise((resolve) => setTimeout(resolve, 160));
    await paint();
    return {
        counter: bounds('.music-counter'),
        number: bounds('#selection-number'),
        total: bounds('.music-counter > div > span'),
        navigation: bounds('.music-navigation'),
        header: bounds('.music-header'),
        brand: bounds('.music-brand'),
        nav: bounds('.music-topnav'),
        theme: bounds('.theme-switch'),
        browse: bounds('.album-callout'),
        footer: bounds('.music-bottomline'),
        play: bounds('#play-pause'),
        stop: bounds('#stop-playback'),
        title: bounds('#selection-title'),
        artist: bounds('#selection-artist'),
        meta: bounds('#selection-meta'),
        open: bounds('.open-album'),
        settingsSvg: bounds('[data-action="settings"] svg'),
        bottom: getComputedStyle(stage).getPropertyValue('--callout-bottom'),
    };
};
window.ready = true;
