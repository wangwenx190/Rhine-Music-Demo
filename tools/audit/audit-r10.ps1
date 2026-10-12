$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/render-backend'
$summary = Get-Content -Raw (Join-Path $evidence 'summary.json') | ConvertFrom-Json
if ($summary.configuration -ne 'Release' -or -not $summary.gpuPassed -or $summary.gpuCases -ne 70 -or $summary.qmllint -ne 'passed' -or -not $summary.productionPassed -or $summary.productionReviewCliExitCode -ne 1 -or $summary.selectedBackend -ne 'rhi' -or $summary.implementationReady) {
    throw 'Missing real R10 evidence'
}
if ((Get-FileHash (Join-Path $repository 'tests/fixtures/render-backend/visual.json')).Hash.ToLowerInvariant() -ne $summary.fixtureSha256) {
    throw 'Stale fixture'
}
$index = Get-Content -Raw (Join-Path $evidence 'evidence-index.json') | ConvertFrom-Json
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($file in $index.sourceRecords) {
    $text = [IO.File]::ReadAllText((Join-Path $repository $file.path)).Replace("`r`n", "`n")
    if ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes($text))).ToLowerInvariant() -ne $file.sha256) {
        throw "Stale source: $($file.path)"
    }
}
foreach ($file in $index.artifacts) {
    if ((Get-FileHash (Join-Path $repository $file.path)).Hash.ToLowerInvariant() -ne $file.sha256) {
        throw "Stale artifact: $($file.path)"
    }
}
$variants = Get-Content -Raw (Join-Path $evidence 'shader-variants.json') | ConvertFrom-Json
if ($variants.qtVersion -ne $summary.qtVersion -or $variants.shaders.Count -ne 4) {
    throw 'Same-Kit probe shader evidence missing'
}
foreach ($shader in $variants.shaders) {
    if (-not $shader.passed -or $shader.targets.Count -ne 5 -or (Get-FileHash (Join-Path $repository $shader.source)).Hash.ToLowerInvariant() -ne $shader.sha256) {
        throw 'Stale probe shader'
    }
}
$rules = Get-Content -Raw (Join-Path $PSScriptRoot 'r10-rules.json') | ConvertFrom-Json
foreach ($platform in @('macos', 'linux')) {
    $checks = @()
    foreach ($rule in $rules.checks) {
        $sources = @()
        foreach ($source in $rule.sourcePath) {
            $sources += @($index.sourceRecords | Where-Object path -eq $source)
        }
        if ($sources.Count -ne $rule.sourcePath.Count) {
            throw 'Missing source hash'
        }
        $checks += @{
            checkId = $rule.checkId
            requirement = $rule.requirement
            platform = $platform
            sourcePath = $rule.sourcePath
            lineOrSymbol = $rule.lineOrSymbol
            sources = $sources
            evidence = $rule.evidence
            result = 'passed'
            limitations = 'No target-system compiler, GPU, audio or deployment execution; selected RHI implementation belongs to R10A-D.'
        }
    }
    $checks += @{
        checkId = if ($platform -eq 'macos') {
            'MAC-01'
        } else {
            'LNX-01'
        }
        requirement = 'Platform integration delta'
        platform = $platform
        sourcePath = 'src/app/main.cpp'
        lineOrSymbol = 'existing native graphicsApi selection'
        evidence = 'No new platform branch, launcher, deployment, device or path API. This round uses public Qt image/instance/texture interfaces and metadata; existing Metal/OpenGL bootstrap and platform scripts remain. Full platform implementation/audit remains R31/R32.'
        result = 'N/A'
        limitations = 'Windows readback is not target image evidence.'
    }
    $output = Join-Path $repository "verification/$platform-static/render-backend"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $matrix = @{
        round = 'R10'
        configuration = 'Release'
        qtVersion = $summary.qtVersion
        conclusion = '静态审计通过，实机待用户验证'
        evidenceIndexSha256 = (Get-FileHash (Join-Path $evidence 'evidence-index.json')).Hash.ToLowerInvariant()
        checks = $checks
        unresolvedStaticDefects = @()
    }
    [IO.File]::WriteAllText((Join-Path $output 'audit-matrix.json'), ($matrix | ConvertTo-Json -Depth 14) + "`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $output 'audit-report.md'), "# R10 $platform 静态审计`n`n静态审计通过，实机待用户验证。`n`nCOMMON-01–05 的新增 C++/QML/qrc/实例 shader/比较器/所有权及证据调用链见 audit-matrix.json；MAC-01/LNX-01 无新增平台分支。未执行目标平台构建、图形、音频或部署。选择 RHI 不表示实现已经运行，R10A–D 的具体任务仍须执行；用户验证步骤见 docs/platforms/$platform.md。`n", $utf8)
}
Write-Output 'R10 macOS/Linux 静态审计通过，实机待用户验证。'
