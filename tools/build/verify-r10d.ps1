[CmdletBinding()]
param([ValidateRange(12, 120)][int]$Seconds = 120, [switch]$ContinueAfterRegression, [ValidateRange(1, 100)][int]$FirstRun = 1)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/rhi-integration'
$media = Join-Path $repository 'docs/media/rhi-integration'
$utf8 = [Text.UTF8Encoding]::new($false)
if (Test-Path -LiteralPath (Join-Path $evidence 'summary.json')) {
    throw 'Preserve prior R10D evidence; use a separate checkout/directory for a new measurement.'
}
New-Item -ItemType Directory -Force -Path $evidence, $media | Out-Null
function Save([string]$Name, [string]$Text) {
    $Text = $Text.Replace($env:USERPROFILE.Replace('\', '/'), '<USERPROFILE>').Replace($env:USERPROFILE, '<USERPROFILE>')
    $Text = $Text.Replace($repository.Replace('\', '/'), '<REPOSITORY>').Replace($repository, '<REPOSITORY>')
    $Text = (($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }) -join "`n").TrimEnd() + "`n"
    $destination = Join-Path $evidence $Name
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    [IO.File]::WriteAllText($destination, $Text, $utf8)
}
function Run([string]$Name, [scriptblock]$Command) {
    $output = & $Command 2>&1
    Save $Name ($output -join "`n")
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed; raw output retained"
    }
    Write-Output "$Name passed"
}
function MeasureIntegration([string]$Backend, [int]$Number, [int]$Duration) {
    $destination = Join-Path $evidence "$Backend/run-$Number"
    if (Test-Path -LiteralPath $destination) {
        throw "Preserve prior raw sample: $destination"
    }
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    $env:QSG_RHI_BACKEND = $Backend
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $repository 'build/release/bin/yalahoo.exe'
    $start.ArgumentList.Add('--verify-integration')
    $start.ArgumentList.Add($destination)
    $start.ArgumentList.Add('--integration-seconds')
    $start.ArgumentList.Add([string]$Duration)
    $start.WorkingDirectory = $repository
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $measurementProcess = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $measurementProcess.StandardOutput.ReadToEndAsync()
        $stderr = $measurementProcess.StandardError.ReadToEndAsync()
        if (-not $measurementProcess.WaitForExit(210000)) {
            $measurementProcess.Kill($true)
            $measurementProcess.WaitForExit()
            throw "Integration measurement timed out: $Backend/$Number"
        }
        Save "$Backend/run-$Number/process.txt" ($stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult())
        $exitCode = $measurementProcess.ExitCode
    } finally {
        $measurementProcess.Dispose()
    }
    if ($exitCode -ne 0) {
        throw "Integration measurement failed: $Backend/$Number (exit $exitCode); raw sample retained"
    }
    $report = Get-Content -Raw (Join-Path $destination 'runtime.json') | ConvertFrom-Json
    $expectedApi = if ($Backend -eq 'd3d11') {
        4
    } else {
        3
    }
    if (-not $report.passed -or -not $report.windowPolicy.passed -or $report.graphicsApi -ne $expectedApi -or $report.vsyncDisabled -or $report.coalescedSamples -ne 0 -or $report.inputRevisionsSubmitted -ne 4) {
        throw 'Incomplete actual backend/integration evidence'
    }
    Save "$Backend/run-$Number/runtime.json" (Get-Content -Raw (Join-Path $destination 'runtime.json'))
    Copy-Item -LiteralPath (Join-Path $destination 'integration.png') -Destination (Join-Path $media "$Backend-$Number.png")
    Write-Host "$Backend/run-$Number passed"
    return @{
        backend = $Backend
        run = $Number
        distributions = $report.distributions
        estimatedInitialBytes = $report.estimatedInitialBytes
        estimatedFinalBytes = $report.estimatedFinalBytes
        estimatedReplacementPeakBytes = $report.estimatedReplacementPeakBytes
    }
}
Push-Location -LiteralPath $repository
$environmentNames = @('PATH', 'QT_PLUGIN_PATH', 'QT_SCALE_FACTOR', 'QT_ENABLE_HIGHDPI_SCALING', 'QSG_RHI_BACKEND', 'QSG_NO_VSYNC', 'QSG_RENDER_LOOP')
$savedEnvironment = @{}
foreach ($name in $environmentNames) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    if (-not $ContinueAfterRegression) {
        Run 'release-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON -DYALAHOO_PERFORMANCE_RECORD_ONLY=ON }
        Run 'release-format.txt' { cmake --build --preset qt-release --target format }
        Run 'release-check-format.txt' { cmake --build --preset qt-release --target check-format }
        & ./tools/format/FormatTools.ps1 -Check
        if (-not $?) {
            throw 'PowerShell style check failed'
        }
        Run 'release-build.txt' { cmake --build --preset qt-release }
        Run 'release-qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
        if ((Get-Content -Raw (Join-Path $evidence 'release-qmllint.txt')) -match '(?m)^(Warning|Error):') {
            throw 'QML diagnostics remain'
        }
    } else {
        $checkpoint = Get-Content -Raw (Join-Path $evidence 'regression-checkpoint.json') | ConvertFrom-Json
        if ($checkpoint.binarySha256 -ne (Get-FileHash build/release/bin/yalahoo.exe).Hash.ToLowerInvariant()) {
            throw 'Regression binary changed; repeat full Review checks'
        }
        foreach ($entry in $checkpoint.sources) {
            if ($entry.sha256 -ne (Get-FileHash -LiteralPath $entry.path).Hash.ToLowerInvariant()) {
                throw "Regression source changed: $($entry.path)"
            }
        }
        if ((Get-Content -Raw (Join-Path $evidence 'release-ctest.txt')) -notmatch '100% tests passed out of 38') {
            throw 'No complete passed Review regression checkpoint'
        }
    }
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    Save 'release-toolchain.json' (Get-Content -Raw build/release/toolchain.json)
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $env:QT_SCALE_FACTOR = '1'
    $env:QT_ENABLE_HIGHDPI_SCALING = '0'
    $env:QSG_RENDER_LOOP = 'threaded'
    Remove-Item Env:QSG_NO_VSYNC -ErrorAction SilentlyContinue
    $env:QSG_RHI_BACKEND = 'd3d11'
    if (-not $ContinueAfterRegression) {
        Run 'release-ctest.txt' { ctest --preset qt-release --output-on-failure }
        $sourceRecords = @()
        foreach ($directory in @('src', 'qml', 'tests', 'cmake', 'resources', 'tools')) {
            foreach ($file in Get-ChildItem -LiteralPath $directory -Recurse -File) {
                if ($directory -eq 'tools' -and $file.Extension -notin @('.cpp', '.h', '.qml')) {
                    continue
                }
                $sourceRecords += @{
                    path = [IO.Path]::GetRelativePath($repository, $file.FullName).Replace('\', '/')
                    sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
                }
            }
        }
        $sourceRecords += @{
            path = 'CMakeLists.txt'
            sha256 = (Get-FileHash CMakeLists.txt).Hash.ToLowerInvariant()
        }
        Save 'regression-checkpoint.json' (@{
                binarySha256 = (Get-FileHash build/release/bin/yalahoo.exe).Hash.ToLowerInvariant()
                sources = $sourceRecords
            } | ConvertTo-Json -Depth 6)
    }
    foreach ($path in @('scene-errors.json', 'lighting-errors.json', 'motion-errors.json', 'runtime.json', 'runtime-motion.json',
            'runtime-library.json', 'materials/runtime.json', 'lighting/runtime.json', 'occlusion/runtime.json',
            'postprocess/gpu-checks.json', 'rhi/runtime.json', 'rhi-shell/runtime.json', 'rhi-postprocess/runtime.json')) {
        Save "regression/$path" (Get-Content -Raw "build/release/verification/$path")
    }
    foreach ($dpr in @('1', '1.5', '2')) {
        $path = "scene-dpr-$dpr/runtime.json"
        Save "regression/$path" (Get-Content -Raw "build/release/verification/$path")
    }
    foreach ($name in @('rhi-shell', 'rhi-postprocess')) {
        $destination = Join-Path $media $name
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        foreach ($file in Get-ChildItem -LiteralPath "build/release/verification/$name" -Filter '*.png') {
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $destination $file.Name)
        }
    }
    $variants = @()
    foreach ($file in Get-ChildItem -LiteralPath build/release/src/render/rhi/.qsb -Filter '*.qsb') {
        $source = Join-Path $repository ('resources/shaders/rhi/' + $file.Name.Replace('.qsb', ''))
        if (-not (Test-Path -LiteralPath $source)) {
            continue
        }
        $dump = & "$($chain.qtKit)/bin/qsb.exe" --dump $file.FullName 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $dump -notmatch 'MSL' -or $dump -notmatch 'HLSL' -or $dump -notmatch 'GLSL' -or $dump -notmatch 'SPIR-V') {
            throw "Missing shader variant: $($file.Name)"
        }
        Save "shaders/$($file.Name).txt" $dump
        $variants += @{
            name = $file.Name
            sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
            sourceSha256 = (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant()
        }
    }
    if ($variants.Count -ne 24) {
        throw 'Expected 24 current RHI shader programs'
    }
    Save 'shader-variants.json' (@{
            shaders = $variants
        } | ConvertTo-Json -Depth 8)
    Run 'adapters.json' { & ./build/release/tools/rhi-postprocess-audit.exe --adapters }
    $binaryHash = (Get-FileHash build/release/bin/yalahoo.exe).Hash.ToLowerInvariant()
    $runs = @()
    for ($number = $FirstRun; $number -lt $FirstRun + 3; ++$number) {
        $runs += MeasureIntegration 'd3d11' $number $Seconds
    }
    $runs += MeasureIntegration 'opengl' 1 12
    $env:QSG_RHI_BACKEND = 'd3d11'
    Run 'production-configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=OFF -DYALAHOO_PERFORMANCE_RECORD_ONLY=OFF }
    Run 'production-build.txt' { cmake --build --preset qt-release }
    Run 'production-dependencies.txt' { dumpbin /dependents build/release/bin/yalahoo.exe }
    if ((Get-Content -Raw (Join-Path $evidence 'production-dependencies.txt')) -match 'Qt6Quick3D') {
        throw 'Production binary imports Quick3D'
    }
    Run 'production-check-format.txt' { cmake --build --preset qt-release --target check-format }
    Run 'production-qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint }
    Run 'production-ctest.txt' { ctest --preset qt-release --output-on-failure }
    if ((Get-Content -Raw build/release/src/app/Yalahoo/Music/Yalahoo.qmltypes) -match 'FixedTextureData') {
        throw 'Production QML type surface contains a Quick3D research type'
    }
    $summary = @{
        round = 'R10D'
        passed = $true
        configuration = 'Release'
        qtVersion = $chain.qtVersion
        reviewBinarySha256 = $binaryHash
        productionBinarySha256 = (Get-FileHash build/release/bin/yalahoo.exe).Hash.ToLowerInvariant()
        operatingSystem = [Environment]::OSVersion.VersionString
        throughputBinarySha256 = (Get-FileHash build/release/tools/rhi-postprocess-audit.exe).Hash.ToLowerInvariant()
        performanceAcceptance = 'record-only'
        performanceCompliance = 'not established'
        throughputReports = @('regression/rhi-postprocess/runtime.json')
        graphicsAdapters = @(Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}' -ErrorAction SilentlyContinue |
                Where-Object PSChildName -Match '^\d{4}$' |
                ForEach-Object {
                    $driver = Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction SilentlyContinue
                    if ($driver.DriverDesc) {
                        $driver | Select-Object @{
                            Name = 'Name'
                            Expression = { $_.DriverDesc }
                        }, DriverVersion
                    }
                })
        graphicsAdapterSource = 'Read-only installed display class registry; native adapter inventory in adapters.json. WMI may be denied by the execution sandbox.'
        runs = $runs
        implementationReady = $true
        scope = 'R06-R10 regression plus one-case production RHI integration. Normal VSYNC distributions are per run/stage diagnostics; full array/search/presentation/scanning acceptance and input budget/noise remain R28. No measured scanout/compositor latency or total physical VRAM.'
        macos = '静态审计通过，实机待用户验证'
        linux = '静态审计通过，实机待用户验证'
    }
    Save 'summary.json' ($summary | ConvertTo-Json -Depth 15)
    & ./tools/audit/audit-r10d.ps1
    if (-not $?) {
        throw 'Static audit failed'
    }
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
    Pop-Location
}
