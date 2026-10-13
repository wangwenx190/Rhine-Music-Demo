import {execFileSync} from 'node:child_process';
import {readFileSync} from 'node:fs';
import path from 'node:path';

const mappings = new Map();

// Provenance keeps the original release identity; Git lookups use its cleaned equivalent.
export function execFrozenGit(args, options = {}) {
    const root = path.resolve(options.cwd || process.cwd());
    if (!mappings.has(root)) {
        const manifest = JSON.parse(
            readFileSync(path.join(root, 'docs/contracts/history-rewrite.json'), 'utf8'),
        );
        for (const [original, rewritten] of Object.entries(manifest.revisions)) {
            if (!/^[a-f0-9]{40}$/.test(original) || !/^[a-f0-9]{40}$/.test(rewritten)) {
                throw new Error('Invalid frozen revision mapping');
            }
        }
        mappings.set(root, manifest.revisions);
    }
    const revisions = mappings.get(root);
    const resolved = args.map((argument) => argument.replace(
                                  /^[a-f0-9]{40}(?=[:~^]|$)/,
                                  (revision) => revisions[revision] || revision,
                                  ));
    return execFileSync('git', resolved, options);
}
