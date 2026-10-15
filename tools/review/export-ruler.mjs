import {execFrozenGit} from './frozen-git.mjs';
import {CDP} from './cdp.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
const root = path.resolve(import.meta.dirname, '../..');
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const frozen = path.join(root, 'build/ruler-web');
const output = path.join(root, 'verification/windows/navigation/web');
await fs.mkdir(path.join(frozen, 'src'), {recursive: true});
await fs.mkdir(output, {recursive: true});
const sources = {};
for (const name of ['music-ruler.ts', 'archive-loop.ts', 'music-motion-settings.ts',
                    'music-navigation-ruler.css']) {
    let text = execFrozenGit(['show', baseline + ':src/' + name], {cwd: root, encoding: 'utf8'});
    sources[name] = createHash('sha256').update(text).digest('hex');
    if (name === 'archive-loop.ts') {
        // Keep the exact two pure helpers used by ruler; other archive functions need the unrelated library.
        text = text.slice(text.indexOf('export type ArchiveCell'),
                          text.indexOf('export function fileAtCell'));
    }
    if (name === 'music-ruler.ts') {
        // Inspection only: expose closure values after the unchanged production update/render path.
        text = text.replace('    destroy() {', `    probe() {
      return { scroll: scroll.value, scrollVelocity: scroll.velocity, width: width.value,
        selected: selectedOrdinal, time: motionTime / 1000,
        slots: slots.map((s) => ({x: s.x, ordinal: s.ordinal, height: s.height.value,
          alpha: Math.max(0, Math.min(1,s.presence.value))*Math.max(0,Math.min(1,s.opacity.value)),
          enabled: !s.button.disabled, active: s.button.classList.contains('active')})) };
    },
    destroy() {`);
    }
    await fs.writeFile(path.join(frozen, 'src', name), text);
}
const {createServer} =
    await import(pathToFileURL(path.join(root, 'legacy-web/node_modules/vite/dist/node/index.js')));
const harness = await fs.readFile(path.join(root, 'tools/review/ruler-harness.js'), 'utf8');
const html =
    '<!doctype html><html><head><meta charset="utf-8"><link rel="stylesheet" href="/src/music-navigation-ruler.css">'
    +
    '<style>body{margin:0}#album-ticks{position:absolute!important;left:2px!important;top:2px!important}button{outline:none}</style>'
    +
    '</head><body><div id="album-ticks"></div><script type="module">' + harness
    + '</script></body></html>';
const server = await createServer({
    root: frozen,
    configFile: false,
    logLevel: 'error',
    server: {host: '127.0.0.1', port: 0},
    plugins: [{
        name: 'ruler',
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
const profile = path.join(root, 'build/ruler-profile-' + process.pid);
await fs.mkdir(profile, {recursive: true});
const processHandle =
    spawn(executable,
          [
              '--headless=new', '--remote-debugging-port=0', '--user-data-dir=' + profile,
              '--no-first-run', '--disable-background-networking', '--disable-component-update',
              '--disable-extensions', '--disable-features=msEdgeSidebarV2,msEdgeStartupBoost',
              'about:blank'
          ],
          {windowsHide: true, stdio: ['ignore', 'ignore', 'pipe']});
processHandle.stderr.on('data', bytes => console.error(bytes.toString()));
processHandle.on('exit', (code, signal) => console.error('Browser exited', code, signal));
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

    const {targetId} = await browser.send('Target.createTarget', {url: 'about:blank'});
    const targets = await (await fetch('http://127.0.0.1:' + port[0] + '/json/list')).json();
    page = await CDP.connect(targets.find((entry) => entry.id === targetId).webSocketDebuggerUrl);
    console.log('Page connected');
    console.log('Enabling Runtime');
    await page.send('Runtime.enable');
    console.log('Runtime enabled');
    page.on('Runtime.exceptionThrown',
            (event) => console.error(JSON.stringify(event.exceptionDetails)));
    console.log('Enabling Page');
    await page.send('Page.enable');
    console.log('Page enabled');
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
    const fixture = JSON.parse(
        await fs.readFile(path.join(root, 'tests/fixtures/navigation/ruler.json'), 'utf8'));
    const captures = [];
    for (const dpr of [1, 1.5, 2]) {
        for (const narrow of [false, true]) {
            for (const night of [false, true]) {
                await page.send('Emulation.setDeviceMetricsOverride', {
                    width: narrow ? 390 : 1280,
                    height: 80,
                    deviceScaleFactor: dpr,
                    mobile: false
                });
                const density = '?density=' + dpr + '&narrow=' + narrow + '&night=' + night;
                await page.send('Page.navigate', {url: origin + '/browse-ui' + density});
                await wait(() => page.evaluate('window.ready && location.search === '
                                               + JSON.stringify(density)
                                               + ' && window.devicePixelRatio === ' + dpr));
                for (const test of fixture.cases) {
                    for (const fps of [20, 30, 60, 120]) {
                        await page.evaluate('window.resetRuler(' + narrow + ',' + night + ')');
                        const states = [];
                        for (let i = 0; i < test.operations.length; ++i) {
                            const op = test.operations[i];
                            const state = await page.evaluate(
                                'window.rulerOperation(' + JSON.stringify(op) + ',' + fps + ','
                                + (dpr === 1 && !narrow && !night && fps === 120) + ')');
                            const capture = fps === 120 && (i === 0 || Boolean(op.frames));
                            states.push({op, state, capture});
                        }
                        captures.push({name: test.name, dpr, narrow, night, fps, states});
                    }
                }
            }
        }
    }
    const dependencyRoot = path.join(root, 'legacy-web/node_modules/@kitlangton/rolling-number');
    const dependencyPackage =
        JSON.parse(await fs.readFile(path.join(dependencyRoot, 'package.json'), 'utf8'));
    const frozenLock = JSON.parse(
        execFrozenGit(['show', baseline + ':package-lock.json'], {cwd: root, encoding: 'utf8'}));
    if (dependencyPackage.version
        !== frozenLock.packages['node_modules/@kitlangton/rolling-number'].version)
        throw Error('Installed rolling-number differs from the frozen lock');
    const dependency =
        await fs.readFile(path.join(dependencyRoot, 'dist/chunk-vm1v7ncb.js'), 'utf8');
    sources.rollingNumber = createHash('sha256').update(dependency).digest('hex');
    const trackSource =
        dependency.slice(dependency.indexOf('function L('), dependency.indexOf('function fe('));
    const sampleSource =
        dependency.slice(dependency.indexOf('function J('), dependency.indexOf('function Q('));
    if (!trackSource.startsWith('function L(') || !sampleSource.startsWith('function J('))
        throw Error('Frozen rolling-number pure track symbols were not found');
    const oracle = Function(trackSource + sampleSource + '; return {track:L, sample:J};')();
    const ordinalSamples = [];
    let track = {target: 0, duration: 0, points: [0, 0]}, elapsed = 0,
        state = {position: 0, velocity: 0};
    const operations = [
        {target: 1, instant: true}, {target: 7}, {dt: .123}, {target: -2}, {dt: .1}, {target: 9},
        {dt: .3}, {target: 9}, {dt: .16}, {target: 0}, {dt: .460}
    ];
    for (const op of operations) {
        if (op.dt !== undefined) {
            elapsed += op.dt * 1000;
            state = oracle.sample(track, elapsed);
        } else if (op.instant || op.target !== track.target) {
            track = oracle.track(state.position, op.target, state.velocity, op.instant ? 0 : 460);
            elapsed = 0;
            state = oracle.sample(track, 0);
        }
        ordinalSamples.push({op, value: state.position, velocity: state.velocity});
    }
    await fs.writeFile(
        path.join(output, 'report.json'),
        JSON.stringify({baseline, sources, version, captures, ordinalSamples}, null, 2) + '\n');
    console.log('Frozen ruler scenarios: ' + captures.length);
} catch (error) {
    console.error('Ruler export failed:', error);
    throw error;
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
