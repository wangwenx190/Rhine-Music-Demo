// Shared frozen TypeScript loading with exact source provenance and explicit dependencies.
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
import path from 'node:path';
import {execFrozenGit, baselineRevision} from './frozen-git.mjs';

export const repositoryRoot = path.resolve(import.meta.dirname, '../..');
export const typescript = createRequire(import.meta.url)(
    path.join(repositoryRoot, 'legacy-web/node_modules/typescript'),
);
export function transpile(source, module = typescript.ModuleKind.CommonJS) {
    return typescript
        .transpileModule(source, {
            compilerOptions: {target: typescript.ScriptTarget.ES2022, module},
        })
        .outputText;
}
export function loadSource(source, dependencies = {}) {
    const exports = {};
    new Function('exports', 'require', transpile(source))(exports, id => {
        if (!(id in dependencies))
            throw new Error(`Unexpected dependency ${id}`);
        return dependencies[id];
    });
    return exports;
}
export class FrozenSource {
    constructor(sources = []) {
        this.sources = sources;
    }
    read(name) {
        const sourcePath = `src/${name}.ts`;
        const source = execFrozenGit(['show', `${baselineRevision}:${sourcePath}`], {
            cwd: repositoryRoot,
            encoding: 'utf8',
        });
        this.sources.push(
            {path: sourcePath, sha256: createHash('sha256').update(source).digest('hex')});
        return source;
    }
    load(name, dependencies = {}) {
        return loadSource(this.read(name), dependencies);
    }
}

export function frozenReader(sources) {
    return name => {
        const text = execFrozenGit(['show', baselineRevision + ':' + name], {
            cwd: repositoryRoot,
            encoding: 'utf8',
        });
        sources[name] = createHash('sha256').update(text).digest('hex');
        return text;
    };
}
