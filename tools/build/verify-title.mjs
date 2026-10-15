import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';

const root = path.resolve(import.meta.dirname, '../..');
const platform = process.platform === 'win32'    ? 'windows'
                 : process.platform === 'darwin' ? 'macos'
                                                 : 'linux';
const options = {
    backend: platform === 'windows' ? 'd3d11'
             : platform === 'macos' ? 'metal'
                                    : 'opengl',
    dpr: 2,
    source: path.join(root, 'verification', platform, 'title-reels/web')
};
let visible = false;
const seen = new Set();
const args = process.argv.slice(2);
for (let i = 0; i < args.length; ++i) {
    const option = args[i];
    if (seen.has(option))
        throw Error('Duplicate option: ' + option);
    seen.add(option);
    if (option === '--visible') {
        visible = true;
    } else {
        if (!['--backend', '--dpr', '--source', '--output'].includes(option) || !args[i + 1])
            throw Error('Usage: verify-title.mjs [--backend metal|d3d11|opengl] [--dpr 1|1.5|2] ' +
                        '[--source directory] [--output directory] [--visible]');
        options[option.slice(2)] = args[++i];
    }
}
const dpr = Number(options.dpr);
if (![1, 1.5, 2].includes(dpr) || !['metal', 'd3d11', 'opengl'].includes(options.backend))
    throw Error('Unsupported backend or DPR');
const source = path.resolve(options.source);
const output = options.output ? path.resolve(options.output)
                              : path.join(root, 'verification', platform, 'title-reels',
                                          options.backend + '-dpr-' + dpr + '-' + Date.now());
if (fs.existsSync(path.join(output, 'report.json')))
    throw Error('Use a fresh output directory to retain earlier evidence');
const reference = JSON.parse(fs.readFileSync(path.join(source, 'report.json'), 'utf8'));
if (reference.captures.filter((capture) => Math.abs(capture.dpr - dpr) < 0.001).length !== 20)
    throw Error('Generate twenty frozen source states at the requested DPR first');
if (reference.protocol !== 'source-structure-font-metrics-motion-v1')
    throw Error('Regenerate source metrics with the current non-image exporter');
if ((reference.platform && reference.platform !== process.platform)
    || (process.platform !== 'win32' && !reference.platform))
    throw Error('Source captures must be generated on this platform');
const cache = fs.readFileSync(path.join(root, 'build/release/CMakeCache.txt'), 'utf8');
const kit = cache.match(/^YALAHOO_SELECTED_QT:PATH=(.+)$/m)?.[1].trim();
if (!kit || !/^CMAKE_BUILD_TYPE:STRING=Release\r?$/m.test(cache))
    throw Error('Configure and build the native qt-release preset first');
const environment = {
    ...process.env,
    PATH: path.join(kit, 'bin') + path.delimiter + process.env.PATH,
    QT_PLUGIN_PATH: path.join(kit, 'plugins'),
    QSG_RHI_BACKEND: options.backend,
    QSG_RENDER_LOOP: process.platform === 'darwin' && options.backend === 'opengl' ? 'basic'
                                                                                   : 'threaded',
    QSG_NO_VSYNC: '',
    QCPAINTER_DEBUG_COLLECT: '0',
    QT_SCALE_FACTOR: '1'
};
environment.YALAHOO_VERIFY_VISIBLE = visible ? '1' : '0';
environment.YALAHOO_VERIFY_FOREGROUND =
    visible ? (process.env.YALAHOO_VERIFY_FOREGROUND || '0') : '0';
const executable =
    path.join(root, 'build/release/tools/ui-audit' + (process.platform === 'win32' ? '.exe' : ''));
fs.mkdirSync(output, {recursive: true});
function run(arguments_, log) {
    const result = spawnSync(executable, arguments_, {
        cwd: root,
        env: environment,
        windowsHide: true,
        encoding: 'utf8',
        maxBuffer: 64 * 1024 * 1024,
        timeout: 25 * 60 * 1000
    });
    fs.writeFileSync(path.join(output, log), (result.stdout || '') + (result.stderr || ''));
    if (result.error || result.signal || result.status === null)
        throw Error('Native audit failed: ' + (result.error || result.signal));
    return result;
}
const native = run(['--native-dpr'], 'native-dpr.txt');
const nativeDpr = Number(native.stdout.trim());
if (native.status !== 0 || !Number.isFinite(nativeDpr) || nativeDpr <= 0)
    throw Error('Native screen DPR query failed');
environment.QT_SCALE_FACTOR = String(dpr / nativeDpr);
const result = run(['--titles', output, source], 'title-audit.txt');
if (/Binding loop detected|Failed to build graphics pipeline|No GLSL shader code found|Render-thread font identity differs|ReferenceError|TypeError|Unable to assign/
        .test((result.stdout || '') + (result.stderr || '')))
    throw Error('QML binding or shader pipeline failure; output retained in ' + output);
if (result.status !== 0)
    throw Error('Title audit exited ' + result.status + '; output retained in ' + output);
const report = JSON.parse(fs.readFileSync(path.join(output, 'report.json'), 'utf8'));
if (visible && (report.offscreen || report.renderProtocol !== 'native-window'))
    throw Error('Visible verification requires an actual native window');
if (process.platform === 'darwin' && !visible
    && (!report.offscreen || report.renderProtocol !== 'render-control-gui-thread'
        || report.offscreenResourcePlan?.nativeWindowVisible !== false
        || report.offscreenResourcePlan?.attachmentEstimateBytesPerPixel !== 12
        || report.offscreenResourcePlan?.actualDriverAllocationBytes !== null))
    throw Error('macOS title verification requires a hidden RenderControl target');
const production = report.captures.filter((capture) => capture.mode === 0);
if (report.backend !== options.backend || Math.abs(report.dpr - dpr) > 0.001 || !report.released
    || !report.windowPolicy.passed || report.motionError > 0.000001 || production.length !== 20
    || report.headerChecks.length !== 18 || report.lifecycle.length !== 10
    || report.animatedCosts.length !== 9)
    throw Error('Backend, DPR, motion, window, capture or lifecycle gates failed');
if (production.some((capture) =>
                        !capture.valid || !capture.geometry.passed || !capture.structurePassed)
    || report.headerChecks.some((check) => !check.passed)
    || report.lifecycle.some((check) => !check.passed))
    throw Error('Structure, header or lifecycle gates failed');
for (const mode of [0, 1, 2]) {
    const samples = report.animatedCosts.filter((sample) => sample.mode === mode);
    if (samples.length !== 3 || new Set(samples.map((sample) => sample.run)).size !== 3)
        throw Error('Three independent cost runs required for each path');
    for (const sample of samples) {
        if (!sample.passed || sample.frames !== 120 || sample.samples.length !== 120
            || sample.gpuTime !== null || sample.guiStateNs == null
            || sample.canvasPaintCpuNs == null || sample.resources.cacheBytes > 8388608
            || sample.resources.cacheEntries > 96 || (mode !== 2 && sample.canvasPaints !== 0))
            throw Error('Cost, cache or hidden Canvas work gate failed');
    }
}
if (!report.passed)
    throw Error('Native checks failed');
const summary = {
    passed: true,
    protocol: 'source-structure-font-metrics-motion-v1',
    visualQuality: 'requires-own-output-inspection',
    platform: process.platform,
    architecture: process.arch,
    configuration: 'Release',
    offscreen: report.offscreen === true,
    renderProtocol: report.renderProtocol || 'native-window',
    qtVersion: report.qtVersion,
    backend: report.backend,
    dpr: report.dpr,
    nativeScreenDpr: nativeDpr,
    scaleFactor: dpr / nativeDpr,
    sourceDirectory: source
};
fs.writeFileSync(path.join(output, 'summary.json'), JSON.stringify(summary, null, 2) + '\n');
console.log(JSON.stringify(summary, null, 2));
console.log('Evidence: ' + output);
