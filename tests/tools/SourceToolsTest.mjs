// Check shared frozen-source boundaries without starting a browser or writing reference fixtures.
import assert from 'node:assert/strict';
import {execFrozenGit, baselineRevision} from '../../tools/review/frozen-git.mjs';
import {FrozenSource, loadSource, frozenReader} from '../../tools/review/frozen-source.mjs';
import {sourceMarkup, sourceHtml} from '../../tools/review/browser-source.mjs';
import {createHash} from 'node:crypto';

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
console.log('Frozen source loading, dependency rejection and markup reconstruction passed');
