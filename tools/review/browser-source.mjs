// Shared frozen source markup and isolated browser transport for development exporters.
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
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
    await server.listen();
    return server;
}
