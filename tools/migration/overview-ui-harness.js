// Inspect frozen production CSS/markup and font boxes; never capture source pixels.
const stage = document.querySelector('#stage');
const surface = document.querySelector('#music-overview');
surface.hidden = false;
const anchor = document.createElement('div');
anchor.className = 'overview-anchor';
anchor.innerHTML = anchorMarkup;
surface.querySelector('.overview-columns').append(anchor);
const card = anchor.querySelector('.overview-column');
card.querySelector('strong').textContent = '晚风与留声机';
card.querySelector('small').textContent = '01 / 12 张专辑';
const bounds = (element, relative) => {
    const rect = element.getBoundingClientRect();
    const origin = relative.getBoundingClientRect();
    const style = getComputedStyle(element);
    const context = document.createElement('canvas').getContext('2d');
    context.font = style.font;
    const font = context.measureText('音乐');
    return {
        x: rect.x - origin.x,
        y: rect.y - origin.y,
        width: rect.width,
        height: rect.height,
        fontSize: parseFloat(style.fontSize),
        fontWeight: style.fontWeight,
        fontAscent: font.fontBoundingBoxAscent,
        fontDescent: font.fontBoundingBoxDescent,
    };
};
window.renderCase = async (input) => {
    stage.dataset.layout = input.width / input.height < 1.05 ? 'portrait' : 'desktop';
    stage.dataset.theme = input.night ? 'night' : 'day';
    stage.dataset.overview = 'true';
    anchor.style.left = input.width / 2 + 'px';
    anchor.style.top = input.height / 2 + 'px';
    await document.fonts.ready;
    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    const variants = [];
    for (const expanded of [false, true]) {
        card.dataset.expanded = String(expanded);
        await new Promise(resolve => requestAnimationFrame(resolve));
        variants.push({
            expanded,
            card: bounds(card, anchor),
            label: bounds(card.querySelector('.overview-column-label'), card),
            name: bounds(card.querySelector('strong'), card),
            count: bounds(card.querySelector('small'), card),
            entry: bounds(card.querySelector('.overview-enter'), card),
        });
    }
    return {variants};
};
window.ready = true;
