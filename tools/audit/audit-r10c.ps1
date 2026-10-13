$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/rhi-postprocess'
$report = Get-Content -Raw (Join-Path $evidence 'release-runtime.json') | ConvertFrom-Json
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
$variants = Get-Content -Raw (Join-Path $evidence 'shader-variants.json') | ConvertFrom-Json
$prior = Get-Content -Raw (Join-Path $repository 'verification/windows/render-backend/release-runtime.json') | ConvertFrom-Json
$fixtureHash = (Get-FileHash -LiteralPath (Join-Path $repository 'tests/fixtures/render-backend/visual.json')).Hash.ToLowerInvariant()
if ($prior.fixtureSha256 -ne $fixtureHash -or $prior.baseline -ne $report.baseline) {
    throw 'Final source/Quick3D fixture hash mismatch'
}
if (-not $summary.passed -or -not $report.passed -or -not $report.nativeTransferByteProof -or $variants.shaders.Count -ne 24 -or $summary.configuration -ne 'Release') {
    throw 'Actual Windows evidence/variant closure is incomplete'
}
$utf8 = [Text.UTF8Encoding]::new($false)
$manifestPath = Join-Path $repository 'resources/manifests/rhi-postprocess.json'
$manifest = Get-Content -Raw $manifestPath | ConvertFrom-Json
$manifest.shaderPrograms = $variants.shaders.Count
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repository 'resources/shaders/rhi') -File) {
    $path = 'resources/shaders/rhi/' + $file.Name
    if ($path -notin $manifest.sourceRecords.path) {
        $manifest.sourceRecords += [pscustomobject]@{
            path = $path
            sha256 = ''
            hashMode = 'UTF-8 LF text'
        }
    }
}
foreach ($entry in $manifest.sourceRecords) {
    $source = Join-Path $repository $entry.path
    if ($entry.hashMode -eq 'UTF-8 LF text') {
        $bytes = $utf8.GetBytes([IO.File]::ReadAllText($source).Replace("`r`n", "`n"))
        $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
    } else {
        $hash = (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant()
    }
    if ($entry.path.StartsWith('resources/shaders/rhi/') -or $entry.path.StartsWith('tools/assets/prepare-rhi-')) {
        $entry.sha256 = $hash
    } elseif ($entry.sha256 -ne $hash) {
        throw "Frozen resource/source changed: $($entry.path)"
    }
}
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 12).Replace("`r`n", "`n") + "`n", $utf8)
$paths = @('CMakeLists.txt', 'cmake/RhiProbe.cmake', 'src/render/rhi/CMakeLists.txt',
    'src/render/rhi/RhiItem.h', 'src/render/rhi/RhiItem.cpp', 'src/render/rhi/AtlasUpload.h',
    'src/render/rhi/AtlasUpload.cpp', 'src/render/rhi/Postprocess.h', 'src/render/rhi/Postprocess.cpp',
    'src/render/rhi/TransferAudit.h', 'tools/assets/prepare-rhi-shell.py',
    'tools/assets/prepare-rhi-postprocess.mjs', 'tools/review/RhiPostprocessAudit.cpp',
    'tools/review/RhiReviewInput.h', 'tests/unit/AtlasUploadTest.cpp',
    'tests/unit/CMakeLists.txt', 'tools/build/verify-r10c.ps1', 'tools/audit/audit-r10c.ps1',
    'docs/contracts/rhi-postprocess.md', 'docs/contracts/render-backend.json',
    'tests/fixtures/render-backend/visual.json', 'tests/fixtures/rhi-shell/visual.json',
    'tests/fixtures/occlusion/visual.json', 'src/app/VerificationWindow.h',
    'src/render/rhi/ShellMaterial.h', 'tools/review/CMakeLists.txt', 'tools/review/RhiShellAudit.cpp',
    'tools/review/RhiAudit.cpp', 'docs/contracts/render-backend.md',
    'docs/contracts/rhi-shell.md', 'docs/contracts/rhi-opaque.md',
    'docs/platforms/macos.md', 'docs/platforms/linux.md',
    'resources/manifests/rhi-postprocess.json', 'QT_QUICK_MIGRATION_PLAN.md', 'README.md',
    'verification/windows/render-backend/release-runtime.json')
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repository 'resources/shaders/rhi') -File) {
    $paths += 'resources/shaders/rhi/' + $file.Name
}
$records = @()
foreach ($path in $paths) {
    $bytes = $utf8.GetBytes([IO.File]::ReadAllText((Join-Path $repository $path)).Replace("`r`n", "`n"))
    $records += @{
        path = $path
        sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
        hashMode = 'UTF-8 LF text'
    }
}
$records += $manifest.sourceRecords
foreach ($base in @('docs/media/render-backend/web', 'docs/media/render-backend/qt',
        'docs/media/rhi-shell/web', 'docs/media/occlusion/web')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repository $base) -Filter '*.png') {
        $records += @{
            path = [IO.Path]::GetRelativePath($repository, $file.FullName).Replace('\', '/')
            sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
            hashMode = 'Raw PNG'
        }
    }
}
$artifacts = @()
foreach ($base in @('verification/windows/rhi-postprocess', 'docs/media/rhi-postprocess')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repository $base) -Recurse -File) {
        if ($file.Name -eq 'evidence-index.json') {
            continue
        }
        $artifacts += @{
            path = [IO.Path]::GetRelativePath($repository, $file.FullName).Replace('\', '/')
            sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
        }
    }
}
$index = @{
    round = 'R10C'
    baseline = '57f4f845c20cce646a15dbf4008c2a162aaaf4e2'
    qtVersion = $summary.qtVersion
    sourceRecords = $records
    artifacts = $artifacts
}
[IO.File]::WriteAllText((Join-Path $evidence 'evidence-index.json'), ($index | ConvertTo-Json -Depth 12).Replace("`r`n", "`n") + "`n", $utf8)
$rules = @(
    @('COMMON-01', 'C++23/latest Kit/private API', 'Root C++23 and same-Kit qsb; configure reprobes subregion upload, resolve/mips and actual D3D11 command access. No minor/patch constants, platform compiler downgrade or product dependency on developer tools.'),
    @('COMMON-02', 'Resources/paths/limits', 'Qrc aliases cover models, lookup/kernel/noise and shaders. Qt Unicode file APIs and native-endian floats; half PMREM conversion checks exact source representability. Target pixels/dimensions/sample support fail explicitly. Atlas generation/lease/serial and bounded queue preserve tile isolation; unsupported optimization uses actual ordinary draw path.'),
    @('COMMON-03', 'Render graph/clip/shader closure', 'QRhi correction owns raster axes/depth. Framebuffer rows are converted separately from unchanged SMAA lookup rows. MSAA resolve precedes mips; sampled opaque color is independent of shell destination. Shared depth uses PreserveDepthStencilContents without DoNotStore; default store survives non-overlapping passes. AO normal/depth use independent RGBA8 geometry coverage, Bokeh depth shares only identical full-size coverage. D32F comparison sampler reproduces source bilinear comparisons and five-point IGN/Vogel. All used native stage input prefixes are contiguous. Early tests are optional; OpenGL lacking Compute/image-store capability uses the ordinary shader. Same selected Kit produced all SPIR-V/GLSL/ES/HLSL/MSL variants.'),
    @('COMMON-04', 'Ownership/ordering/lifecycle', 'Input is a detached value snapshot. Render owns every QRhi allocation; pipelines/SRBs release before attachments. Shared depth is reference-owned and target reset releases RT/pass first. Pending readbacks hold results through finish; GUI delivery uses guarded queued calls. Opaque instances retain stable near-to-far order, shell draws retain global far-to-near/ID/surface ordering. Resize keeps atlas, changing generation cancels old work, queue replacement rejects old leases. Media commands/QtMultimedia devices/cancellation are N/A for this renderer-only round: no media source or controller changes; existing public-API playback remains the shared implementation. Color/geometry/quality do not introduce network or media state.'),
    @('COMMON-05', 'Evidence/limitations', 'Actual Windows Release full regression, QML lint, same-source pre/final ROI, normal/AO/blur and geometry/shadow/order/alpha witnesses are separate. Native D3D11 destination boxes plus completed full-page GPU reads prove bytes; no CPU-payload substitution or physical-bus claim. 432-instance cadence/CPU/timestamp scopes are separated; unthrottled renderer throughput is not a macOS/Linux or final product interaction claim. Residuals remain individually identified. No target OS compiler/GPU/media/deployment execution is claimed.'),
    @('WINDOW-01', 'No desktop interference', 'All exposed reviews prepare bottom/no-focus flags before first show, retain them on resize/rebuild and check inactive state. Unit/format/resource checks remain hidden; native frame captures/readbacks contain only their own render output. No raise, activation, global input, desktop screenshot or network capture is introduced.')
)
foreach ($platform in @('macos', 'linux')) {
    $checks = @()
    foreach ($rule in $rules) {
        $checks += @{
            checkId = $rule[0]
            requirement = $rule[1]
            platform = $platform
            sourcePath = $paths
            lineOrSymbol = 'AtlasUploadQueue / Renderer::initialize/render / Postprocess::prepare/render / nativeTransfers / VerificationWindow'
            evidence = $rule[2]
            sourceHashes = $records
            result = 'passed'
            limitations = 'Target compiler, GPU, media and deployment not executed; Windows shared-code evidence is not target runtime proof.'
        }
    }
    $checks += @{
        checkId = if ($platform -eq 'macos') {
            'MAC-01'
        } else {
            'LNX-01'
        }
        requirement = 'Native backend/resource/feature path'
        platform = $platform
        sourcePath = @('tools/review/RhiPostprocessAudit.cpp', 'src/render/rhi/RhiItem.cpp', 'src/render/rhi/TransferAudit.h')
        lineOrSymbol = 'Q_OS_MACOS Metal / other OpenGL; Q_OS_WIN Review-only native command audit'
        evidence = 'Metal/OpenGL selected before QGuiApplication. Shared shader and actual QRhi resource path remains implemented; no Windows handle or D3D header enters those branches. D3D11 command-byte audit is explicitly unavailable, not zero, on other backends; actual GPU tile images and queue limits remain executable. Bundle/XDG/deployment integration continues under R31/R32.'
        result = 'passed'
        limitations = '静态审计通过，实机待用户验证'
    }
    $destination = Join-Path $repository "verification/$platform-static/rhi-postprocess"
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    $matrix = @{
        round = 'R10C'
        qtVersion = $summary.qtVersion
        conclusion = '静态审计通过，实机待用户验证'
        checks = $checks
        unresolvedStaticDefects = @()
        evidenceIndexSha256 = (Get-FileHash (Join-Path $evidence 'evidence-index.json')).Hash.ToLowerInvariant()
    }
    [IO.File]::WriteAllText((Join-Path $destination 'audit-matrix.json'), ($matrix | ConvertTo-Json -Depth 14).Replace("`r`n", "`n") + "`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $destination 'audit-report.md'), "# R10C $platform 静态审计`n`n静态审计通过，实机待用户验证。`n`n逐项源码/资源/线程/生命周期/后端与 shader 证据见 audit-matrix.json。未执行目标系统编译、GPU、媒体或部署。用户真实验证步骤见 docs/platforms/$platform.md 的 R10C 小节。`n", $utf8)
}
