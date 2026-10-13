import {execFrozenGit} from './frozen-git.mjs';
import {execFileSync} from 'node:child_process';
import {readFileSync, writeFileSync, mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2';
const source = execFrozenGit(['show', `${baseline}:src/music-search.ts`], {
    cwd: root,
    encoding: 'utf8',
});
// Remove only TypeScript declarations; execute the frozen search function body verbatim.
const body = source.slice(source.indexOf('export function searchMusicLibrary'))
                 .replace('export function', 'function')
                 .replace('albums: MusicAlbum[], genres: MusicGenre[], input: string',
                          'albums, genres, input')
                 .replace('): MusicSearchResult[]', ')')
                 .replace('const results: MusicSearchResult[]', 'const results');
const search = new Function(`${body}; return searchMusicLibrary;`)();
const fixtureBytes = readFileSync(path.join(root, 'tests/fixtures/web-baseline/anonymous.json'));
const {libraries} = JSON.parse(fixtureBytes);
const cases = [];
for (const library of Object.keys(libraries)) {
    for (const query of ['',
                         '  ',
                         ' Typography ',
                         '匿名歌手 1',
                         '匿名列 02',
                         '👩🏽‍🚀',
                         'é',
                         'é',
                         '收藏样本',
                         'Ａ',
                         'missing',
                         '未分类',
    ]) {
        for (const genreId of ['', 'fixture-genre-1', 'missing']) {
            const input = libraries[library];
            cases.push({
                library,
                query,
                genreId,
                results: search(input.albums, input.genres, query, genreId)
                             .map((hit) => ({
                                      albumId: hit.album.id,
                                      trackId: hit.track?.id ?? '',
                                  })),
            });
        }
    }
}
const output = path.join(root, 'tests/fixtures/core');
mkdirSync(output, {recursive: true});
writeFileSync(
    path.join(output, 'search-golden.json'),
    JSON.stringify(
        {
            baseline,
            sourcePath: 'src/music-search.ts',
            sourceSha256: createHash('sha256').update(source).digest('hex'),
            fixtureSha256: createHash('sha256').update(fixtureBytes).digest('hex'),
            cases,
        },
        null,
        2,
        )
        + '\n',
);
console.log(`Exported ${cases.length} frozen search cases`);
