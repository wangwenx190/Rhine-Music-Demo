// Shared frozen source markup and isolated browser transport for development exporters.
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {CDP} from './cdp.mjs';
import {execFrozenGit, baselineRevision} from './frozen-git.mjs';
import {createHash} from 'node:crypto';
import {pathToFileURL} from 'node:url';
import {repositoryRoot} from './frozen-source.mjs';

export const sourceStyles = [
    'style', 'music', 'music-navigation-motion', 'music-navigation-ruler', 'music-transport-title',
    'music-theme', 'music-theme-switch', 'music-overview'
];

export function sourceMarkup(appSource) {
    const template =
        appSource.split('stage.innerHTML = `')[1].split('`;')[0].replace(' as Theme[]', '');
    const svg = (body) =>
        '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        + body + '</svg>';
    const iconSource = appSource.split('const icons = ')[1].split('\n};')[0] + '\n}';
    const icons = Function('svg', 'return (' + iconSource + ')')(svg);
    return Function('icons', 'svg', 'preferences', 'themeNames', 'sortLabel', 'appVersion',
                    'return `' + template
                        + '`')(icons, svg, {theme: 'day'}, {day: '暖昼', night: '深夜'},
                               {name: '音乐', column: '类型', code: 'GENRE'}, '0.4.1');
}
export function sourceHtml(markup, harness) {
    return '<!doctype html><html><head><meta charset="utf-8">'
           + sourceStyles.map(name => '<link rel="stylesheet" href="/src/' + name + '.css">')
                 .join('')
           + '<style>.music-app:not([data-layout="portrait"]) .theme-switch span{display:inline}</style>'
           +
           '</head><body><div id="stage" class="music-app reduce-motion" data-mode="archive" data-theme="day">'
           + markup + '</div><script type="module">' + harness + '</script></body></html>';
}
export async function installedBrowser() {
    const candidates = process.platform === 'win32'
        ? [path.join(process.env.PROGRAMFILES || '', 'Google/Chrome/Application/chrome.exe'),
           path.join(process.env['PROGRAMFILES(X86)'] || '', 'Microsoft/Edge/Application/msedge.exe')]
        : process.platform === 'darwin'
        ? ['/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
           '/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge',
           path.join(process.env.HOME || '', 'Applications/Google Chrome.app/Contents/MacOS/Google Chrome')]
        : ['/usr/bin/google-chrome', '/usr/bin/chromium', '/usr/bin/chromium-browser'];
    for (const candidate of (process.env.REVIEW_BROWSER ? [process.env.REVIEW_BROWSER]
                                                        : candidates)) {
        try {
            await fs.access(candidate);
            return candidate;
        } catch {
        }
    }
    throw Error('Installed Chrome/Edge/Chromium required; REVIEW_BROWSER selects its executable');
}
export async function launchBrowser(profile, {diagnostics = false} = {}) {
    const executable = await installedBrowser();
    await fs.mkdir(profile, {recursive: true});
    return spawn(executable,
                 [
                     '--headless=new',
                     '--remote-debugging-port=0',
                     '--user-data-dir=' + profile,
                     '--no-first-run',
                     '--disable-background-networking',
                     '--disable-component-update',
                     '--disable-extensions',
                     '--disable-features=msEdgeSidebarV2,msEdgeStartupBoost',
                     'about:blank',
                 ],
                 {windowsHide: true, stdio: diagnostics ? ['ignore', 'ignore', 'pipe'] : 'ignore'});
}
export async function wait(probe) {
    const deadline = Date.now() + 60000;
    while (Date.now() < deadline) {
        try {
            const value = await probe();
            if (value)
                return value;
        } catch {
        }
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    throw Error('Browser capture timeout');
}

export async function sourceServer(root, html) {
    const {createServer} = await import(pathToFileURL(
        path.join(repositoryRoot, 'legacy-web/node_modules/vite/dist/node/index.js')));
    const server = await createServer({
        root,
        configFile: false,
        logLevel: 'error',
        server: {host: '127.0.0.1', port: 0},
        plugins: [{
            name: 'frozen-source',
            configureServer(instance) {
                instance.middlewares.use('/browse-ui', (_, response) => {
                    response.setHeader('Content-Type', 'text/html; charset=utf-8');
                    response.end(html);
                });
            }
        }],
    });
    try {
        await server.listen();
        return server;
    } catch (error) {
        await server.close();
        throw error;
    }
}

// Reconstruct the common browse/title dependencies before applying exporter-specific probes.
export async function writeSourceUi(root, source, sources, transform = (_, text) => text) {
    await fs.mkdir(path.join(root, 'src'), {recursive: true});
    await fs.mkdir(path.join(root, 'public/fonts'), {recursive: true});
    for (const name of sourceStyles) {
        await fs.writeFile(path.join(root, 'src', name + '.css'), source('src/' + name + '.css'));
    }
    for (const name of ['music-title', 'music-title-reels', 'html', 'music-motion-ui',
                        'music-motion-settings']) {
        await fs.writeFile(path.join(root, 'src', name + '.ts'),
                           transform(name, source('src/' + name + '.ts')));
    }
    for (const weight of ['Light', 'Regular', 'Demibold', 'Bold']) {
        const name = 'public/fonts/MiSans-' + weight + '.woff2';
        const bytes = execFrozenGit(['show', baselineRevision + ':' + name],
                                    {cwd: repositoryRoot, maxBuffer: 16 * 1024 * 1024});
        sources[name] = createHash('sha256').update(bytes).digest('hex');
        await fs.writeFile(path.join(root, name), bytes);
    }
}

export async function rollingNumberSource(sources) {
    const root = path.join(repositoryRoot, 'legacy-web/node_modules/@kitlangton/rolling-number');
    const installed = JSON.parse(await fs.readFile(path.join(root, 'package.json'), 'utf8'));
    const lock = JSON.parse(execFrozenGit(['show', baselineRevision + ':package-lock.json'],
                                          {cwd: repositoryRoot, encoding: 'utf8'}));
    if (installed.version !== lock.packages['node_modules/@kitlangton/rolling-number'].version)
        throw Error('Installed rolling-number differs from the frozen lock');
    const text = await fs.readFile(path.join(root, 'dist/chunk-vm1v7ncb.js'), 'utf8');
    sources.rollingNumber = createHash('sha256').update(text).digest('hex');
    return text;
}

// The session owns server, browser, transports and temporary profile, including partial startup.
export async function withSourcePage(root, html, profile, run,
                                     {diagnostics = false, domains = []} = {}) {
    let server, processHandle, browser, page, ownedProfile;
    try {
        server = await sourceServer(root, html);
        const origin = 'http://127.0.0.1:' + server.httpServer.address().port;
        ownedProfile = await fs.mkdtemp(profile + '-');
        processHandle = await launchBrowser(ownedProfile, {diagnostics});
        let launchError;
        processHandle.on('error', error => {
            launchError = error;
        });
        if (diagnostics) {
            processHandle.stderr.on('data', bytes => console.error(bytes.toString()));
            processHandle.on('exit',
                             (code, signal) => console.error('Browser exited', code, signal));
        }
        const port = (await wait(async () => {
            if (launchError || processHandle.exitCode !== null || processHandle.signalCode !== null)
                return {error: launchError || Error('Browser exited before accepting connections')};
            return {text: await fs.readFile(path.join(ownedProfile, 'DevToolsActivePort'), 'utf8')};
        }));
        if (port.error)
            throw port.error;
        const [number, endpoint] = port.text.split('\n');
        browser = await CDP.connect('ws://127.0.0.1:' + number + endpoint);
        const version = await browser.send('Browser.getVersion');
        const {targetId} = await browser.send('Target.createTarget', {url: 'about:blank'});
        const targets = await (await fetch('http://127.0.0.1:' + number + '/json/list')).json();
        page = await CDP.connect(targets.find(entry => entry.id === targetId).webSocketDebuggerUrl);
        page.on('Runtime.exceptionThrown',
                event => console.error(JSON.stringify(event.exceptionDetails)));
        for (const domain of ['Runtime', 'Page', ...domains])
            await page.send(domain + '.enable');
        await page.send('Fetch.enable', {patterns: [{urlPattern: '*'}]});
        page.on('Fetch.requestPaused', event => {
            const local = event.request.url.startsWith(origin + '/')
                          || /^(data|blob):/.test(event.request.url);
            void page.send(
                local ? 'Fetch.continueRequest' : 'Fetch.failRequest',
                {requestId: event.requestId, ...(local ? {} : {errorReason: 'BlockedByClient'})});
        });
        await page.send('Page.navigate', {url: origin + '/browse-ui'});
        await wait(() => page.evaluate('window.ready'));
        return await run({page, version, origin});
    } finally {
        page?.close();
        if (browser) {
            await browser.send('Browser.close')
                .catch(() => {
                       });
            browser.close();
        }
        if (processHandle?.pid && processHandle.exitCode === null
            && processHandle.signalCode === null) {
            const exited = new Promise(resolve => processHandle.once('exit', resolve));
            processHandle.kill();
            await exited;
        }
        await server?.close();
        if (ownedProfile)
            await fs.rm(ownedProfile, {recursive: true, force: true});
    }
}
