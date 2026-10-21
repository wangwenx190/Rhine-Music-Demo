import {frozenReader} from './frozen-source.mjs';
import {typescript as ts} from './frozen-source.mjs';
import {sourceServer, launchBrowser, wait, sourceMarkup, sourceStyles, sourceHtml} from './browser-source.mjs';
import {transpile} from './frozen-source.mjs';
import {execFrozenGit, baselineRevision} from './frozen-git.mjs';
import {CDP} from './cdp.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';

const root = path.resolve(import.meta.dirname, '../..');
const overview = process.argv.includes('--overview');
const presentation = process.argv.includes('--presentation');
const baseline = baselineRevision;
const frozen = path.join(root, 'build/browse-ui-web/legacy-web');
const output = path.join(root, presentation ? 'verification/presentation-source'
                               : overview   ? 'verification/overview-source'
                                            : 'verification/windows/browse-ui/web');
await fs.mkdir(output, {recursive: true});
await fs.mkdir(path.join(frozen, 'src'), {recursive: true});
await fs.mkdir(path.join(frozen, 'public/fonts'), {recursive: true});
const sources = {};
const source = frozenReader(sources);
const appSource = source('src/music-app.ts');
const markup = sourceMarkup(appSource);
const styles = sourceStyles;
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
let harness = await fs.readFile(path.join(root, 'tools/review/'
                                                    + (presentation ? 'presentation-ui-harness.js'
                                                       : overview   ? 'overview-ui-harness.js'
                                                                    : 'browse-ui-harness.js')),
                                'utf8');
if (presentation) {
    const select = (text, names) => {
        const file = ts.createSourceFile('source.ts', text, ts.ScriptTarget.Latest, true);
        return file.statements
            .filter(node => ts.isFunctionDeclaration(node) && names.includes(node.name?.text))
            .map(node => node.getText(file))
            .join('\n')
            .replaceAll('export function', 'function');
    };
    const functions = select(appSource, [
        'renderDetail', 'trackList', 'albumAbout', 'introductionAlbumStatus', 'producerBlock',
        'groupProducers', 'producerSource', 'creditsCoolingDown', 'creditsAlbumStatus',
        'valueRange', 'formatList', 'albumDuration', 'time'
    ]);
    const titles = select(source('src/music-title.ts'), ['albumTitleParts', 'albumTitleMarkup']);
    harness = ts.transpileModule(functions + '\n' + titles, {
                    compilerOptions: {target: ts.ScriptTarget.ES2022}
                }).outputText
              + '\n' + harness;
}
if (overview) {
    const ui = source('src/music-overview-ui.ts');
    const anchorMarkup = ui.match(/anchor\.innerHTML = '([^']+)'/)[1];
    harness = 'const anchorMarkup = ' + JSON.stringify(anchorMarkup) + ';\n' + harness;
}
const html = sourceHtml(markup, harness);
const server = await sourceServer(frozen, html);
const origin = 'http://127.0.0.1:' + server.httpServer.address().port;
const profile = path.join(root, 'build/browse-ui-profile-' + process.pid);
const processHandle = await launchBrowser(profile);
let browser, page;
try {
    const admissions = [];
    const nativePaths = process.argv.flatMap(
        (arg, index) => arg === '--overview-native' ? [process.argv[index + 1]] : []);
    for (const nativePath of overview ? nativePaths : []) {
        const native = JSON.parse(await fs.readFile(nativePath, 'utf8'));
        const js = transpile(source('src/music-overview.ts'));
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
