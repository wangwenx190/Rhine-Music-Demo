import {execFrozenGit, baselineRevision} from './frozen-git.mjs';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = async (file) => JSON.parse(await fs.readFile(path.join(root, file), 'utf8'));
const hash = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex');
const expected = baselineRevision;
const fixtureBytes = await fs.readFile(
    path.join(root, 'tests/fixtures/web-baseline/anonymous.json'),
);
const fixture = JSON.parse(fixtureBytes), fixtureHash = hash(fixtureBytes);
assert.equal(fixture.baseline, expected);
assert.deepEqual(Object.keys(fixture.libraries), [
    'empty',
    'single',
    'even',
    'odd',
    'mixed',
    'large',
]);
assert.ok(fixture.libraries.large.albums.length > 432);
for (const library of Object.values(fixture.libraries)) {
    assert.equal(library.roots.length, 0);
    assert.equal(library.onlineEnabled, false);
    for (const album of library.albums) {
        assert.equal(album.folder, 'Generated fixture');
        assert.match(album.coverUrl, /^\/demo-covers\/(square|portrait|landscape)\.png$/);
    }
}
const source = await read('docs/contracts/source-map.json');
const names = execFrozenGit(['ls-tree', '-r', '--name-only', expected, 'src'], {
                  cwd: root,
                  encoding: 'utf8',
              })
                  .trim()
                  .split('\n');
assert.deepEqual(
    source.modules.map((m) => m.baselinePath),
    names,
);
for (const entry of source.modules) {
    const bytes = execFrozenGit(['show', expected + ':' + entry.baselinePath], {cwd: root});
    assert.equal(hash(bytes), entry.sha256, entry.baselinePath);
    assert.equal(hash(await fs.readFile(path.join(root, entry.sourcePath))), entry.sha256,
                 entry.sourcePath);
    assert.ok(entry.responsibility);
    if (entry.role === 'migrate')
        assert.ok(entry.target);
}
for (const entry of [...source.modules, ...source.services]) {
    if (entry.target) {
        for (const target of entry.target.split(' + ')) {
            assert.ok(!path.isAbsolute(target) && !target.split('/').includes('..'), target);
            assert.ok(await fs.stat(path.join(root, target)), target);
        }
    }
}
// Resource hashes establish provenance; no screenshots, GPU witnesses or scene dumps.
const assets = await read('resources/manifests/assets.json');
assert.equal(assets.baselineCommit, expected);
assert.ok(assets.entries.length > 0);
const inputs = new Map();
for (const entry of assets.entries) {
    const bytes = await fs.readFile(path.join(root, 'resources', entry.path));
    assert.equal(bytes.length, entry.bytes, entry.path);
    assert.equal(hash(bytes), entry.sha256, entry.path);
    assert.equal(entry.qrc, ':/assets/' + entry.path);
    if (inputs.has(entry.source))
        assert.equal(inputs.get(entry.source), entry.inputSha256, entry.source);
    inputs.set(entry.source, entry.inputSha256);
    if (entry.operation === 'unchanged copy')
        assert.equal(entry.sha256, entry.inputSha256, entry.path);
}
for (const entry of assets.additionalInputs) {
    if (inputs.has(entry.source))
        assert.equal(inputs.get(entry.source), entry.sha256, entry.source);
    inputs.set(entry.source, entry.sha256);
}
assert.ok(inputs.size > 0);
for (const [sourcePath, sha256] of inputs) {
    assert.ok(sourcePath.startsWith('legacy-web/'), sourcePath);
    const bytes = execFrozenGit(['show', expected + ':' + sourcePath.slice(11)],
                                {cwd: root, maxBuffer: 64 * 1024 * 1024});
    assert.equal(hash(bytes), sha256, sourcePath);
    assert.equal(hash(await fs.readFile(path.join(root, sourcePath))), sha256, sourcePath);
}
const branding = await read('resources/manifests/branding.json');
assert.equal(branding.displayName, 'yalahOO!!!');
assert.ok(branding.entries.length > 0);
for (const entry of branding.entries) {
    const bytes = await fs.readFile(path.join(root, entry.path));
    assert.equal(bytes.length, entry.bytes, entry.path);
    assert.equal(hash(bytes), entry.sha256, entry.path);
}
const order = await read('docs/contracts/library-order.json');
assert.equal(order.fixtureHash, fixtureHash);
for (const [name, library] of Object.entries(fixture.libraries)) {
    assert.equal(order.libraries[name].albumIds.length, library.albums.length);
    assert.equal(
        new Set(order.libraries[name].columns.flatMap((c) => c.albumIds)).size,
        library.albums.length,
    );
}
const search = await read('tests/fixtures/core/search-golden.json');
assert.equal(search.baseline, expected);
assert.equal(search.fixtureSha256, fixtureHash);
assert.equal(search.sourceSha256,
             hash(execFrozenGit(['show', expected + ':' + search.sourcePath], {cwd: root})));
assert.equal(search.cases.length, 216);
// Bind committed numerical inputs to their immutable source provenance.
for (const file
         of ['tests/fixtures/motion/kernel-golden.json', 'tests/fixtures/scene/camera-golden.json',
             'tests/fixtures/archive-pool/golden.json',
             'tests/fixtures/real-columns/trajectories.json',
             'tests/fixtures/real-columns/fields.json',
             'tests/fixtures/materials/parameters.json']) {
    const input = await read(file);
    assert.equal(input.baseline, expected, file);
    for (const source of input.sources) {
        assert.equal(hash(execFrozenGit(['show', expected + ':' + source.path], {cwd: root})),
                     source.sha256, file + ': ' + source.path);
    }
}
const lighting = await read('tests/fixtures/lighting/golden.json');
assert.equal(lighting.baseline, expected);
for (const [file, sha256] of Object.entries(lighting.sources))
    assert.equal(hash(execFrozenGit(['show', expected + ':' + file], {cwd: root})), sha256, file);
const result = {
    baseline: expected,
    fixtureHash,
    sourceModules: source.modules.length,
    assetEntries: assets.entries.length,
    frozenAssetInputs: inputs.size,
    brandEntries: branding.entries.length,
    scope: 'Frozen source and immutable asset/fixture provenance; no runtime or visual acceptance',
    result: 'passed',
};
console.log(JSON.stringify(result, null, 2));
