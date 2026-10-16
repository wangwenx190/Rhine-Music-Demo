// Optional frozen-Node semantics check; native builds and CTest never need Node.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {mkdtemp, rm, stat, utimes, writeFile} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';

const binary = path.resolve(process.argv[2]
                            ?? `build/release/tests/unit/DirectoryScannerTest${
                                process.platform === 'win32' ? '.exe' : ''}`);
const temporary = await mkdtemp(path.join(tmpdir(), 'yalahoo-scan-'));
try {
    const file = path.join(temporary, '歌曲 空格 # %.FLAC');
    await writeFile(file, 'anonymous fixture');
    // Use the actual filesystem stat after setting time: filesystem precision varies.
    for (const stamp
             of [null, 1700000000.123456, 1700000000.987654, 1234567890.000001, 0, 1.999999]) {
        if (stamp !== null)
            await utimes(file, stamp, stamp);
        const info = await stat(file);
        const result =
            JSON.parse(execFileSync(binary, ['--fingerprint', file], {encoding: 'utf8'}));
        assert.equal(result.fingerprint, `${info.size}:${info.mtimeMs}`);
        assert.equal(result.id,
                     `track-${createHash('sha256').update(file).digest('hex').slice(0, 24)}`);
    }
    console.log('Native UTF-8 IDs and size:mtimeMs match frozen Node semantics (6 timestamps)');
} finally {
    await rm(temporary, {recursive: true, force: true});
}
