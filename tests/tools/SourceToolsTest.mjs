// Check shared frozen-source boundaries without starting a browser or writing reference fixtures.
import assert from 'node:assert/strict';
import {execFrozenGit, baselineRevision} from '../../tools/review/frozen-git.mjs';
import * as frozenSourceModule from '../../tools/review/frozen-source.mjs';
import * as frozenGitModule from '../../tools/review/frozen-git.mjs';
import {FrozenSource, loadSource, frozenReader, repositoryRoot as repository} from '../../tools/review/frozen-source.mjs';
import {sourceMarkup, sourceHtml, writeSourceUi, rollingNumberSource} from '../../tools/review/browser-source.mjs';
import {createHash} from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';

assert.equal(loadSource('export const amount: number = 7;').amount, 7);
assert.throws(() => loadSource('import x from \'./missing\'; export const value = x;'),
              /Unexpected dependency/);
const records = [];
const text = new FrozenSource(records).read('motion');
assert.equal(records.length, 1);
assert.equal(records[0].path, 'src/motion.ts');
assert.equal(records[0].sha256, createHash('sha256').update(text).digest('hex'));
const sources = {};
const app = frozenReader(sources)('src/music-app.ts');
assert.equal(app, execFrozenGit(['show', `${baselineRevision}:src/music-app.ts`],
                                {encoding: 'utf8', cwd: repository}));
const markup = sourceMarkup(app);
assert.ok(markup.includes('album-ticks'));
assert.ok(markup.includes('0.4.1'));
assert.ok(!markup.includes('${'));
assert.ok(sourceHtml(markup, 'fixture()').includes('<script type="module">fixture()</script>'));
const detached = await fs.mkdtemp(path.join(os.tmpdir(), 'yalahoo-source-'));
try {
    const provenance = {};
    const probe = '\n// source fixture probe\n';
    await writeSourceUi(detached, frozenReader(provenance), provenance,
                        (name, text) => name === 'music-title' ? text + probe : text);
    for (const [name, hash] of Object.entries(provenance)) {
        let bytes = await fs.readFile(path.join(detached, name));
        if (name === 'src/music-title.ts') {
            assert.ok(bytes.toString().endsWith(probe));
            bytes = bytes.subarray(0, bytes.length - Buffer.byteLength(probe));
        }
        assert.equal(createHash('sha256').update(bytes).digest('hex'), hash);
    }
    const rolling = await rollingNumberSource(provenance);
    assert.equal(provenance.rollingNumber, createHash('sha256').update(rolling).digest('hex'));
} finally {
    await fs.rm(detached, {recursive: true, force: true});
}
console.log('Frozen source loading, dependency rejection, UI reconstruction and provenance passed');

// Evaluate current exporters with captured file writes. Checked-in mathematical fixtures
// remain an independent oracle; the regression test never rewrites them.
// Source motion contracts allow at most 1e-6 double-state error across JS engines.
const motionTolerance = 1e-6;
function assertOracle(actual, expected, location) {
    if (typeof expected === 'number') {
        assert.ok(Number.isFinite(actual) && Math.abs(actual - expected) <= motionTolerance,
                  `${location}: ${actual} differs from ${expected}`);
    } else if (expected && typeof expected === 'object') {
        assert.deepEqual(Object.keys(actual), Object.keys(expected), location);
        for (const key of Object.keys(expected))
            assertOracle(actual[key], expected[key], `${location}.${key}`);
    } else {
        assert.equal(actual, expected, location);
    }
}
for (const [exporter,
            fixture] of [['export-scene-golden.mjs', 'scene/camera-golden.json'],
                         ['export-real-columns-golden.mjs', 'real-columns/trajectories.json'],
]) {
    const writes = new Map();
    const sourcePath = path.join(repository, 'tools/review', exporter);
    const sourceUrl = (await import('node:url')).pathToFileURL(sourcePath).href;
    const source = (await fs.readFile(sourcePath, 'utf8'))
                       .replaceAll('import.meta.url', JSON.stringify(sourceUrl));
    const dependencies = {
        './frozen-source.mjs': frozenSourceModule,
        './frozen-git.mjs': frozenGitModule,
        'node:fs': {
            mkdirSync() {},
            writeFileSync(file, bytes) {
                writes.set(file, JSON.parse(bytes));
            },
        },
    };
    for (const name of ['node:module', 'node:url', 'node:path'])
        dependencies[name] = await import(name);
    loadSource(source, dependencies);
    const expectedPath = path.join(repository, 'tests/fixtures', fixture);
    const expected = JSON.parse(await fs.readFile(expectedPath, 'utf8'));
    assertOracle(writes.get(expectedPath), expected, exporter);
}
console.log(
    'Camera and finite-column exporters reproduce independent fixtures without file writes');
