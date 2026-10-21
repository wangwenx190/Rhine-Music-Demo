// Check shared frozen-source boundaries without starting a browser or writing reference fixtures.
import assert from 'node:assert/strict';
import {execFrozenGit, baselineRevision} from '../../tools/review/frozen-git.mjs';
import {FrozenSource, loadSource, frozenReader} from '../../tools/review/frozen-source.mjs';
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
assert.equal(app,
             execFrozenGit(['show', `${baselineRevision}:src/music-app.ts`], {encoding: 'utf8'}));
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
