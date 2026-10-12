import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
export async function exportLibraryOrder(root, frozen, fixture, baseline, fixtureHash) {
    const data = await import(pathToFileURL(path.join(frozen, 'src/data.ts')).href);
    const libraries = {};
    for (const [name, library] of Object.entries(fixture.libraries)) {
        data.setMusicAlbums(library.albums, library.genres, 'genre');
        libraries[name] = {
            sortMode: 'genre',
            albumIds: data.records.map((r) => r.album.id),
            columns: data.archiveColumns.map(
                (label, lane) => ({
                    lane,
                    label,
                    albumIds: data.columnFiles(lane).map((i) => data.records[i].album.id),
                })),
            slotStride: data.slotStride,
        };
    }
    const result = {
        baseline,
        fixtureHash,
        source: 'Frozen src/data.ts:setMusicAlbums + src/music-order.ts:orderMusicAlbums',
        libraries,
    };
    await fs.writeFile(
        path.join(root, 'docs/contracts/library-order.json'),
        JSON.stringify(result, null, 2) + '\n',
    );
    return result;
}
