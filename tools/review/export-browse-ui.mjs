import {execFrozenGit} from './frozen-git.mjs';
import {CDP} from './cdp.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';

const root = path.resolve(import.meta.dirname, '../..');
const overview = process.argv.includes('--overview');
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const frozen = path.join(root, 'build/browse-ui-web/legacy-web');
const output = path.join(root, overview ? 'verification/overview-source'
                                        : 'verification/windows/browse-ui/web');
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
    await fs.writeFile(path.join(frozen, 'src', name + '.ts'), source('src/' + name + '.ts'));
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
let harness = await fs.readFile(
    path.join(root,
              'tools/review/' + (overview ? 'overview-ui-harness.js' : 'browse-ui-harness.js')),
    'utf8');
if (overview) {
    const ui = source('src/music-overview-ui.ts');
    const anchorMarkup = ui.match(/anchor\.innerHTML = '([^']+)'/)[1];
    harness = 'const anchorMarkup = ' + JSON.stringify(anchorMarkup) + ';\n' + harness;
}
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
             '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
             '/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge',
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
const profile = path.join(root, 'build/browse-ui-profile-' + process.pid);
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
    const admissions = [];
    const nativePaths = process.argv.flatMap(
        (arg, index) => arg === '--overview-native' ? [process.argv[index + 1]] : []);
    for (const nativePath of overview ? nativePaths : []) {
        const native = JSON.parse(await fs.readFile(nativePath, 'utf8'));
        const ts =
            createRequire(import.meta.url)(path.join(root, 'legacy-web/node_modules/typescript'));
        const js =
            ts.transpileModule(source('src/music-overview.ts'), {
                  compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS},
              }).outputText;
        const exports = {};
        new Function('exports', 'require', js)(exports, name => {
            if (name !== 'three' && name !== './motion.ts')
                throw Error('Unexpected overview dependency: ' + name);
            return {};
        });
        let retained = new Set();
        for (const capture of native.captures) {
            if (capture.expanded < 0)
                retained.clear();
            const lanes =
                exports
                    .visibleOverviewLabels(capture.columns, capture.width, capture.height, retained,
                                           capture.expanded < 0 ? null : capture.expanded)
                    .map(c => c.lane);
            admissions.push({
                width: capture.width,
                height: capture.height,
                night: capture.night,
                dpr: capture.dpr,
                expanded: capture.expanded,
                backend: native.backend,
                lanes
            });
            retained = new Set(lanes);
        }
    }
    const port = (await wait(() => fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')))
                     .split('\n');
    browser = await CDP.connect('ws://127.0.0.1:' + port[0] + port[1]);
    console.log('Browser connected');
    const version = await browser.send('Browser.getVersion');
    console.log('Browser version read');
    const {targetId} = await browser.send('Target.createTarget', {url: 'about:blank'});
    const targets = await (await fetch('http://127.0.0.1:' + port[0] + '/json/list')).json();
    page = await CDP.connect(targets.find((entry) => entry.id === targetId).webSocketDebuggerUrl);
    console.log('Page connected');
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
    for (const dpr of (process.argv.includes('--dpr-1') ? [1] : [1, 1.5, 2])) {
        for (const [width, height] of [[1920, 1080], [1280, 720], [995, 858], [960, 900],
                                       [930, 900], [390, 844], [320, 720], [2560, 1080],
                                       [3440, 1440]]) {
            for (const night of [false, true]) {
                await page.send('Emulation.setDeviceMetricsOverride',
                                {width, height, deviceScaleFactor: dpr, mobile: false});
                const input = {width, height, dpr, night};
                const bounds =
                    await page.evaluate('window.renderCase(' + JSON.stringify(input) + ')');
                captures.push({...input, ...bounds});
            }
        }
    }
    await fs.writeFile(path.join(output, 'report.json'),
                       JSON.stringify({baseline, sources, version, captures, admissions}, null, 2)
                           + '\n');
    console.log('Frozen browse UI: ' + captures.length + ' states');
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
