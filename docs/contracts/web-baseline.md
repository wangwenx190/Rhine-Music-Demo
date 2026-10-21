# 冻结Web来源与输入重建

本文只管来源映射、匿名输入、导出与校验；外观优先级/禁止像素追分由迁移计划规定，组件算法见对应契约。

The reference is commit `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` (V0.4.1), music entry `src/main.ts → src/music-app.ts`. `legacy-web/` remains the asset and dependency source. Review code extracts that commit into ignored `build/web-baseline/legacy-web/`; it never edits the archived source or starts the old music service. The Vite/API fixture server binds to loopback, has no scanner, and returns only generated JSON. Browser storage and profile are isolated under `build/web-baseline/`. External page requests are rejected and reported. Audio and BGM are disabled.

Rebuild from the repository root:

```powershell
Push-Location legacy-web
npm ci --ignore-scripts --no-audit --no-fund --allow-remote=all
Pop-Location
node tools/review/export-web-baseline.mjs
node tools/review/check-web-baseline.mjs
```

`--allow-remote=all` is needed by npm 12 for the existing lockfile's tarball mirror URLs; this does not add packages or change the lockfile. The exporter verifies the lockfile against the frozen commit. No browser is downloaded. It discovers installed Chrome/Edge/Chromium or accepts `--browser=PATH` / `REVIEW_BROWSER`. `--repo-root=PATH` and `--web-root=PATH` are independent inputs. Build/check subprocesses run with the detached Web root as cwd and an explicit isolated `MUSIC_DATA_DIR`. Node is a review dependency; it is not a future native application runtime dependency.

`tests/fixtures/web-baseline/anonymous.json` is shared scene input: viewport, DPR, theme, quality, array mode, stable album IDs, selection, motion time and events. It contains empty/single/even/odd/mixed/480-album libraries, three bundled demo cover shapes, Chinese long titles, descenders, parentheses, emoji and combining characters. Roots are empty; synthetic tracks have no audio URLs. No user music, browser preferences, index or absolute music paths are read.

`node tools/review/native-source-map.mjs` updates native destinations independently of Web capture; the same mapping is used by the baseline exporter. `source-map.json` enumerates every frozen `src/` module with content hash, imports, music reachability and new responsibility; service modules have separate scanner/provider/audio policies. Reachability includes shared archive geometry/shaders and the viewer; reference-only modules remain archived. `library-order.json` calls the frozen `setMusicAlbums/orderMusicAlbums` functions and records actual stable album IDs and column order for each fixture. Visual Qt fixtures must use this order rather than relying on platform collator differences. The new application must split composition, navigation, models, motion, rendering, media and offline metadata rather than transplant `music-app.ts` wholesale.

`scene-parameters.json` is a hash-checked index into `web-scenes/*.json`, which store final camera matrices, light world transforms, effective material properties, texture settings, compositor order and active AO/DOF/AA, instance transforms, paper geometry/UV, computed UI typography/layout/visibility/CSS variables and brand rectangles. State settles before capture. Values come from real initialized objects, not constructor defaults. Geometry and the shared fixed atlas UV layout are separately sampled from actual runtime objects. Invalid/missing required fields fail verification.

Review-only Vite transforms expose private JS object state and append an application frame driver. Production functions and rendering algorithms remain unchanged. The FPS HUD follows this synthetic clock and is not GPU or performance timing evidence. Frames and Web Animations advance at explicit 1/120-second intervals; actual animation completion promises/events are flushed between tasks. Rendering happens only at samples, with visible textures prepared and uploads flushed. Resource preparation uses readiness predicates; screenshot timing never uses a sleep as scene-state evidence. `motion-golden.json` calls frozen analytical functions for damping, quintics/rebase/reversal, real columns, ribbon motion/field, overview folding, glyph tracks and opening; 20/30/60/120-fps and long-frame inputs are recorded. Each numeric sample specifies dt and events; events apply before that input frame update. Glyph start/velocity/target, spring rates and retarget/rebase inputs are explicit. `presentation-golden.json` samples the actual application switch/reverse/back sequence at 1/120 second, including camera and returning-copy state.

Optional historical captures: screenshots.json identifies baseline/fixture/browser/backend/size/DPR/quality/time; room-environment.json records RoomEnvironment/PMREM input and row order. They are local reference outputs, not required by the basic checker or native runtime. PMREM bytes do not define an acceptance oracle; source orientation/precision and shading are covered by the rendering contracts.

For a focused parameter/sequence refresh from an already verified screenshot index, use `node tools/review/export-web-baseline.mjs --refresh-contracts`; it verifies the same commit/fixture and scene hashes, replays opening/switching and regenerates numerical/environment inputs. A failed export removes the previous success report; it cannot pass the checker using a stale report.

Basic provenance checking is independent of capture reconstruction. `node tools/review/check-web-baseline.mjs` works directly from committed source/assets/numerical inputs and the cleaned frozen revision; it requires no browser, screenshot index, scene dump or GPU readback. It validates archived source hashes, original/prepared resource hashes, branding and anonymous/numerical input constraints, and prints its scoped result without writing a success report. Run `export-web-baseline.mjs` only when reference capture/reconstruction itself is needed; its capture success never certifies native source fidelity. Component source attribution is indexed in [source-fidelity-review.md](source-fidelity-review.md); remaining work is owned by the migration plan.

冻结revision由`resources/manifests/assets.json::baselineCommit`统一提供；`tools/review/frozen-git.mjs`解析清理后映射，`frozen-source.mjs`统一TypeScript配置、显式依赖与来源hash，`browser-source.mjs`共享原markup重建、后台浏览器启动和本地来源服务。`node tests/tools/SourceToolsTest.mjs`检查这些开发工具的依赖拒绝与重建边界，不启动浏览器。
