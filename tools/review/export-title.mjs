import {execFrozenGit} from './frozen-git.mjs';
import {CDP} from './cdp.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';

const root = path.resolve(import.meta.dirname, '../..');
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const frozen = path.join(root, 'build/title-web/legacy-web');
const output = path.join(root, 'verification/windows/title-reels/web');
await fs.mkdir(output, {recursive: true});
await fs.mkdir(path.join(frozen, 'src'), {recursive: true});
await fs.mkdir(path.join(frozen, 'public/fonts'), {recursive: true});
const sources = {};
const source = (name) => {
    const text = execFrozenGit(['show', baseline + ':' + name], {cwd: root, encoding: 'utf8'});
    sources[name] = createHash('sha256').update(text).digest('hex');
    return text;
};
const appSource = source('src/music-app.ts');
const template =
    appSource.split('stage.innerHTML = `')[1].split('`;')[0].replace(' as Theme[]', '');
const svg = (body) =>
    '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
    + body + '</svg>';
const iconSource = appSource.split('const icons = ')[1].split('\n};')[0] + '\n}';
const icons = Function('svg', 'return (' + iconSource + ')')(svg);
const markup =
    Function('icons', 'svg', 'preferences', 'themeNames', 'sortLabel', 'appVersion',
             'return `' + template + '`')(icons, svg, {theme: 'day'}, {day: '暖昼', night: '深夜'},
                                          {name: '音乐', column: '类型', code: 'GENRE'}, '0.4.1');
const styles = [
    'style', 'music', 'music-navigation-motion', 'music-navigation-ruler', 'music-transport-title',
    'music-theme', 'music-theme-switch', 'music-overview'
];
for (const name of styles) {
    // Reconstruct source assets from the frozen revision, never use mutable Web files.
    await fs.writeFile(path.join(frozen, 'src', name + '.css'), source('src/' + name + '.css'));
}
for (const name of ['music-title', 'music-title-reels', 'html', 'music-motion-ui',
                    'music-motion-settings']) {
    await fs.writeFile(
        path.join(frozen, 'src', name + '.ts'),
        name === 'music-title'
            ? source('src/' + name + '.ts')
                  .replace(
                      'candidate.root.style.display = "block";\n    return candidate;',
                      'candidate.root.style.display = "block";\n    const fresh = candidate.root.cloneNode(true); candidate.root.replaceWith(fresh); candidate.root = fresh; candidate.main = fresh.children[0]; candidate.translation = fresh.children[1];\n    return candidate;')
                  .replace(
                      'return { size: candidate.size, height: Math.min(y, budget), glyphs };',
                      'window.lastTitleLayout = { size: candidate.size, height: Math.min(y, budget), glyphs, rootInlineFont: candidate.root.style.fontSize, rootFont: getComputedStyle(candidate.root).fontSize, mainFont: getComputedStyle(candidate.main).fontSize }; return window.lastTitleLayout;')
        : name === 'music-title-reels'
            ? source('src/' + name + '.ts') + '\nwindow.SourceTitleTrack = TitleTrack;\n'
            : source('src/' + name + '.ts'));
}
for (const weight of ['Light', 'Regular', 'Demibold', 'Bold']) {
    const name = 'public/fonts/MiSans-' + weight + '.woff2';
    const bytes =
        execFrozenGit(['show', baseline + ':' + name], {cwd: root, maxBuffer: 16 * 1024 * 1024});
    sources[name] = createHash('sha256').update(bytes).digest('hex');
    await fs.writeFile(path.join(frozen, name), bytes);
}
const {createServer} =
    await import(pathToFileURL(path.join(root, 'legacy-web/node_modules/vite/dist/node/index.js')));
const harness = await fs.readFile(path.join(root, 'tools/review/title-harness.js'), 'utf8');
const html =
    '<!doctype html><html><head><meta charset="utf-8">'
    + styles.map((name) => '<link rel="stylesheet" href="/src/' + name + '.css">').join('')
    + '<style>.music-app:not([data-layout="portrait"]) .theme-switch span{display:inline}</style>' +
    '</head><body><div id="stage" class="music-app reduce-motion" data-mode="archive" data-theme="day">'
    + markup + '</div><script type="module">' + harness + '</script></body></html>';
const server = await createServer({
    root: frozen,
    configFile: false,
    logLevel: 'error',
    server: {host: '127.0.0.1', port: 0},
    plugins: [{
        name: 'browse-ui',
        configureServer(s) {
            s.middlewares.use('/browse-ui', (_, response) => {
                response.setHeader('Content-Type', 'text/html; charset=utf-8');
                response.end(html);
            });
        }
    }]
});
await server.listen();
const origin = 'http://127.0.0.1:' + server.httpServer.address().port;
let executable;
for (const candidate
         of [process.env.REVIEW_BROWSER,
             path.join(process.env.PROGRAMFILES || '', 'Google/Chrome/Application/chrome.exe'),
             path.join(process.env['PROGRAMFILES(X86)'] || '',
                       'Microsoft/Edge/Application/msedge.exe')]
             .filter(Boolean)) {
    try {
        await fs.access(candidate);
        executable = candidate;
        break;
    } catch {
    }
}
if (!executable)
    throw Error('Installed browser required');
const profile = path.join(root, 'build/title-profile-' + process.pid);
await fs.mkdir(profile, {recursive: true});
const processHandle =
    spawn(executable,
          [
              '--headless=new', '--remote-debugging-port=0', '--user-data-dir=' + profile,
              '--no-first-run', '--disable-background-networking', '--disable-component-update',
              '--disable-extensions', 'about:blank'
          ],
          {windowsHide: true, stdio: 'ignore'});
async function wait(probe) {
    const deadline = Date.now() + 60000;
    while (Date.now() < deadline) {
        try {
            const value = await probe();
            if (value)
                return value;
        } catch {
        }
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
    throw Error('Browser capture timeout');
}
let browser, page;
try {
    const port = (await wait(() => fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')))
                     .split('\n');
    browser = await CDP.connect('ws://127.0.0.1:' + port[0] + port[1]);
    console.log('Browser connected');
    const version = await browser.send('Browser.getVersion');
    console.log('Browser version read');
    const targets = await (await fetch('http://127.0.0.1:' + port[0] + '/json/list')).json();
    const target = targets.find((entry) => entry.type === 'page');
    const {sessionId} =
        await browser.send('Target.attachToTarget', {targetId: target.id, flatten: true});
    page = {
        send: (method, params) => browser.send(method, params, sessionId),
        on: (method, callback) => browser.on(method, callback),
        evaluate: CDP.prototype.evaluate,
        close: () => {
        }
    };
    console.log('Page connected');
    console.log('Enabling Runtime');
    await page.send('Runtime.enable');
    page.on('Runtime.exceptionThrown',
            (event) => console.error(JSON.stringify(event.exceptionDetails)));
    await page.send('Page.enable');
    await page.send('Fetch.enable', {patterns: [{urlPattern: '*'}]});
    page.on('Fetch.requestPaused', (e) => {
        const local = e.request.url.startsWith(origin + '/') || /^(data|blob):/.test(e.request.url);
        void page.send(
            local ? 'Fetch.continueRequest' : 'Fetch.failRequest',
            {requestId: e.requestId, ...(local ? {} : {errorReason: 'BlockedByClient'})});
    });
    await page.send('Page.navigate', {url: origin + '/browse-ui'});
    console.log('Frozen browse harness navigating');
    await wait(() => page.evaluate('window.ready'));
    const captures = [];
    const fixture =
        JSON.parse(await fs.readFile(path.join(root, 'tests/fixtures/text/titles.json'), 'utf8'));
    for (const dpr of [1, 1.5, 2]) {
        await page.send('Emulation.setDeviceMetricsOverride',
                        {width: 1280, height: 720, deviceScaleFactor: dpr, mobile: false});
        for (const input of fixture.cases) {
            for (const night of [false, true]) {
                const state = await page.evaluate('window.renderCase('
                                                  + JSON.stringify({...input, night}) + ')');
                const file = 'title-' + captures.length + '.png';
                const png = await page.send('Page.captureScreenshot', {
                    format: 'png',
                    clip: {x: 0, y: 0, width: input.width, height: input.budget, scale: 1}
                });
                await fs.writeFile(path.join(output, file), Buffer.from(png.data, 'base64'));
                captures.push({...input, dpr, night, state, file});
                await page.send('Emulation.setDefaultBackgroundColorOverride',
                                {color: {r: 0, g: 0, b: 0, a: 0}});
                await page.evaluate('window.setAlpha(true)');
                const alphaFile = 'alpha-' + file;
                const alpha = await page.send('Page.captureScreenshot', {
                    format: 'png',
                    clip: {x: 0, y: 0, width: input.width, height: input.budget, scale: 1}
                });
                await fs.writeFile(path.join(output, alphaFile), Buffer.from(alpha.data, 'base64'));
                captures.at(-1).alphaFile = alphaFile;
                await page.send('Emulation.setDeviceMetricsOverride', {
                    width: Math.max(1280, input.width * 8 + 4),
                    height: Math.max(720, input.budget * 8 + 4),
                    deviceScaleFactor: dpr,
                    mobile: false
                });
                await page.evaluate('window.mountOutline()');
                const outlineFile = 'outline-' + file;
                const outline = await page.send('Page.captureScreenshot', {
                    format: 'png',
                    clip: {x: 0, y: 0, width: input.width * 8, height: input.budget * 8, scale: 1}
                });
                await fs.writeFile(path.join(output, outlineFile),
                                   Buffer.from(outline.data, 'base64'));
                captures.at(-1).outlineFile = outlineFile;
                await page.evaluate('window.removeOutline()');
                await page.send('Emulation.setDeviceMetricsOverride',
                                {width: 1280, height: 720, deviceScaleFactor: dpr, mobile: false});
                await page.evaluate('window.setAlpha(false)');
                await page.send('Emulation.setDefaultBackgroundColorOverride', {});
            }
        }
    }
    const tracks = await page.evaluate('window.sampleTracks()');
    const dependencyRoot = path.join(root, 'legacy-web/node_modules/@kitlangton/rolling-number');
    const dependencyPackage =
        JSON.parse(await fs.readFile(path.join(dependencyRoot, 'package.json'), 'utf8'));
    const lock = JSON.parse(
        execFrozenGit(['show', baseline + ':package-lock.json'], {cwd: root, encoding: 'utf8'}));
    if (dependencyPackage.version
        !== lock.packages['node_modules/@kitlangton/rolling-number'].version)
        throw Error('Installed rolling-number differs from frozen lock');
    const dependency =
        await fs.readFile(path.join(dependencyRoot, 'dist/chunk-vm1v7ncb.js'), 'utf8');
    sources.rollingNumber = createHash('sha256').update(dependency).digest('hex');
    const part = (from, to) => dependency.slice(dependency.indexOf(from), dependency.indexOf(to));
    const oracle =
        Function(part('function me(', 'function W(') + part('function L(', 'function fe(')
                 + part('function fe(', 'function J(') + part('function J(', 'function Q(')
                 + ';return {entry:me,track:L,blur:fe,sample:J};')();
    const entry = oracle.entry(1, 460);
    const entryBlur = oracle.blur(entry, 0, 6);
    const roll = oracle.track(.12, 2, 12, 460);
    const rolling = [];
    for (let i = 0; i <= 60; ++i) {
        const ms = i * 1000 / 120;
        rolling.push({
            dt: i / 120,
            entry: oracle.sample(entry, ms),
            entryBlur: oracle.sample(entryBlur, ms),
            roll: oracle.sample(roll, ms)
        });
    }
    await fs.writeFile(path.join(output, 'report.json'), JSON.stringify({
        baseline,
        sources,
        version,
        captures,
        tracks,
        rolling,
        probeRefresh:
            'Fresh source probe root with identical inline styles prevents Chromium contain/style returning a prior coarse font after a fine-size write. Raw unrefreshed evidence stays in web-raw; no algorithm, parameter or product style is changed.',
        contour:
            'Reference-only eightfold source clone for a fixed one-physical-pixel glyph contour, including color-font layer boundaries.'
    },
                                                                        null, 2)
                                                             + '\n');
    console.log('Frozen title states: ' + captures.length);

} finally {
    page?.close();
    if (browser) {
        await browser.send('Browser.close')
            .catch(() => {
                   });
        browser.close();
    }
    processHandle.kill();
    await server.close();
}
