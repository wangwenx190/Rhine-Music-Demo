// Actual frozen renderDetail/trackList/albumAbout functions, final CSS, no source pixels.
const stage = document.querySelector('#stage');
const $ = selector => document.querySelector(selector);
const esc = text =>
    String(text).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('"', '&quot;');
const escapeHtml = esc;
const demo = false, selected = 0, apiAvailable = false, creditsStarting = false;
const creditsRequestError = '', creditsRequestAlbumId = '';
const introductionsStarting = false;
const library = {}, preferences = {
    reduced: true
};
const trackFocus = {
    cancel() {}
},
      documentDecryption = {
          reset() {}
      };
const scene = null;
let activeTab = 'tracks', detailIdentity = '';
const syncAlbumNavigation = () => {
}, syncTabIndicator = () => {
}, updatePlayingRows = () => {
};
const genreName = () => '未分类';
const album = {
    id: 'anonymous-detail',
    title: '收藏的声音 · Anonymous Archive',
    artist: '匿名音乐人',
    description: '离线收藏中的专辑介绍。',
    folder: '',
    rawGenres: [],
    producers: [],
    tracks: [{
        id: 'anonymous-track-0',
        title: '收藏曲目 1 🎵',
        artist: '匿名音乐人',
        duration: 185,
        format: 'FLAC',
        discNumber: 1
    }]
};
album.tracks = Array.from({length: 10000}, (_, i) => ({
                                               ...album.tracks[0],
                                               id: 'anonymous-track-' + i,
                                               title: '收藏曲目 ' + (i + 1) + ' 🎵',
                                               discNumber: i < 5000 ? 1 : 2
                                           }));
const currentAlbum = () => album;
const bounds = (element, origin) => {
    const rect = element.getBoundingClientRect(), base = origin.getBoundingClientRect();
    const style = getComputedStyle(element);
    return {
        x: rect.x - base.x,
        y: rect.y - base.y,
        width: rect.width,
        height: rect.height,
        fontSize: parseFloat(style.fontSize),
        gap: parseFloat(style.columnGap) || 0,
        rowGap: parseFloat(style.rowGap) || 0,
        paddingTop: parseFloat(style.paddingTop),
        paddingBottom: parseFloat(style.paddingBottom)
    };
};
const articleReset = () => {
    $('#album-detail-content').scrollTop = 0;
};
window.renderCase = async input => {
    stage.dataset.layout = input.width / input.height < 1.05 ? 'portrait' : 'desktop';
    stage.dataset.theme = input.night ? 'night' : 'day';
    stage.dataset.mode = 'detail';
    $('#music-detail').hidden = false;
    document.documentElement.getBoundingClientRect();
    await new Promise(resolve => requestAnimationFrame(resolve));
    renderDetail();
    articleReset();
    $('#detail-card-id').textContent = 'ALBUM 001';
    await document.fonts.ready;
    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    const article = $('#album-detail-content');
    const fields = {
        article: bounds(article, stage),
        caption: bounds($('.card-caption'), stage),
        title: bounds(article.querySelector('h1'), article),
        artist: bounds($('.detail-artist'), article),
        facts: bounds($('.album-facts'), article),
        factLabel: bounds($('.album-facts small'), article),
        factValue: bounds($('.album-facts span'), article),
        tabs: bounds($('.music-tabs'), article),
        track: bounds($('.track-row'), article),
        trackTitle: bounds($('.track-name strong'), article),
        trackArtist: bounds($('.track-name small'), article)
    };
    article.scrollTop = article.scrollHeight;
    await new Promise(resolve => requestAnimationFrame(resolve));
    fields.stickyTop =
        $('.music-tabs').getBoundingClientRect().top - article.getBoundingClientRect().top;
    return {fields, layout: stage.dataset.layout, computedLeft: getComputedStyle(article).left};
};
window.ready = true;
