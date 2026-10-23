#requires -Version 7.2
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
if (-not $IsWindows) {
    throw 'Windows deployment requires a native Windows MSVC/Qt environment.'
}
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repository 'tools/build/enter-msvc.ps1')
$buildDirectory = Join-Path $repository 'build/windows-release'
$output = Join-Path $repository 'out/windows'
$staging = Join-Path $repository ('out/windows-staging-' + [guid]::NewGuid().ToString('N'))
$scanRoot = Join-Path $buildDirectory 'deploy-scan/qml'
$encoding = [Text.UTF8Encoding]::new($false)

function Invoke-NativeTool([string]$Program, [string[]]$ToolArguments) {
    & $Program @ToolArguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE."
    }
}

Push-Location -LiteralPath $repository
try {
    # Re-discover the newest native Kit, even when the build is already current.
    Invoke-NativeTool cmake @('--preset', 'qt-release', '-B', $buildDirectory,
        '-DYALAHOO_REVIEW=OFF', '-DYALAHOO_MACOS_VISIBLE_TESTS=OFF')
    Invoke-NativeTool cmake @('--build', $buildDirectory, '--target', 'Yalahoo')
    $metadata = Get-Content -LiteralPath (Join-Path $buildDirectory 'deployment.json') -Raw |
        ConvertFrom-Json
    if ($metadata.configuration -ne 'Release' -or $metadata.review -or
        $metadata.system -ne 'Windows' -or $metadata.compiler -ne 'MSVC') {
        throw 'Only the native MSVC production Release target may be packaged.'
    }
    $qtBin = Join-Path $metadata.qtKit 'bin'
    $deployTool = Join-Path $qtBin 'windeployqt.exe'
    $qtPaths = Join-Path $qtBin 'qtpaths.exe'
    foreach ($tool in @($deployTool, $qtPaths)) {
        if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
            throw "Selected Kit tool is missing: $tool"
        }
        $versionArguments = if ($tool -eq $qtPaths) {
            @('--query', 'QT_VERSION')
        } else {
            @('--version')
        }
        $versionText = & $tool @versionArguments 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $versionText -notmatch '(\d+\.\d+\.\d+)' -or
            $Matches[1] -ne $metadata.qtVersion) {
            throw "Deployment tools must match Qt $($metadata.qtVersion): $versionText"
        }
    }
    # Copy only QML compiled into the executable, preserving source-root paths.
    # Scanning all source files would also discover the isolated Quick3D Review.
    if (Test-Path -LiteralPath $scanRoot) {
        Remove-Item -LiteralPath $scanRoot -Recurse -Force
    }
    foreach ($relative in $metadata.qmlFiles) {
        if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[/\\])\.\.([/\\]|$)') {
            throw "Invalid production QML path: $relative"
        }
        $source = Join-Path (Join-Path $repository 'qml') $relative
        $destination = Join-Path $scanRoot $relative
        $null = New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force
        Copy-Item -LiteralPath $source -Destination $destination
    }
    # Only the official architecture-matched redistributable installer is shipped.
    # Never copy a developer machine's loose compiler-runtime DLLs.
    $architecture = switch -Regex ($metadata.architecture) {
        '^(AMD64|x86_64|x64)$' {
            'x64'
            break
        }
        '^(ARM64|aarch64)$' {
            'arm64'
            break
        }
        default {
            throw "Unsupported native Windows architecture: $($metadata.architecture)"
        }
    }
    $redistRoot = Join-Path $env:VCINSTALLDIR 'Redist/MSVC'
    $redist = Get-ChildItem -LiteralPath $redistRoot -Filter "vc_redist.$architecture.exe" -Recurse -File |
        Sort-Object { $_.VersionInfo.FileVersionRaw } -Descending | Select-Object -First 1
    if (-not $redist) {
        throw 'Install the official Visual C++ Redistributable with the selected Visual Studio tools.'
    }
    $null = New-Item -ItemType Directory -Path $staging
    $program = Join-Path $staging 'yalahoo.exe'
    Copy-Item -LiteralPath $metadata.executable -Destination $program
    # Limit Qt lookup to the selected Kit without changing the user's environment.
    $savedPath = $env:PATH
    $savedPluginPath = $env:QT_PLUGIN_PATH
    $savedQmlPath = $env:QML_IMPORT_PATH
    $savedQml2Path = $env:QML2_IMPORT_PATH
    try {
        $env:PATH = $qtBin + [IO.Path]::PathSeparator + $savedPath
        $env:QT_PLUGIN_PATH = Join-Path $metadata.qtKit 'plugins'
        $env:QML_IMPORT_PATH = Join-Path $metadata.qtKit 'qml'
        $env:QML2_IMPORT_PATH = $null
        Invoke-NativeTool $deployTool @('--release', '--dir', $staging, '--qmldir', $scanRoot,
            '--qml-deploy-dir', (Join-Path $staging 'qml'), '--qtpaths', $qtPaths,
            '--no-compiler-runtime', '--skip-plugin-types', 'qmltooling',
            '--include-plugins', 'qsvg,qsvgicon', $program)
    } finally {
        $env:PATH = $savedPath
        $env:QT_PLUGIN_PATH = $savedPluginPath
        $env:QML_IMPORT_PATH = $savedQmlPath
        $env:QML2_IMPORT_PATH = $savedQml2Path
    }
    [IO.File]::WriteAllText((Join-Path $staging 'qt.conf'),
        "[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml`n", $encoding)
    $prerequisites = New-Item -ItemType Directory -Path (Join-Path $staging 'prerequisites')
    Copy-Item -LiteralPath $redist.FullName -Destination $prerequisites.FullName
    $licenses = New-Item -ItemType Directory -Path (Join-Path $staging 'licenses')
    Copy-Item -LiteralPath (Join-Path $repository 'resources/licenses') -Destination (Join-Path $licenses 'project') -Recurse
    Copy-Item -LiteralPath (Join-Path $repository 'resources/fonts/NOTICE.txt') -Destination (Join-Path $licenses 'MiSans-NOTICE.txt')
    Copy-Item -LiteralPath (Join-Path $repository 'resources/fonts/MiSans-license.pdf') -Destination $licenses
    Copy-Item -LiteralPath (Join-Path $repository 'resources/audio/README.md') -Destination (Join-Path $licenses 'audio.md')
    . (Join-Path $PSScriptRoot 'qt-licenses.ps1')
    Copy-QtLicenses -Kit $metadata.qtKit -Destination (Join-Path $licenses 'Qt')
    $readme = @"
# yalahOO!!!

Install prerequisites/$($redist.Name) if the required Visual C++ runtime is absent.
Start yalahoo.exe for your local music library; yalahoo.exe --demo uses anonymous albums.
Qt libraries, plugins, fonts, shaders and media dependencies are included in this directory.
Keep the directory together when copying it to another Windows machine.

With PowerShell 7.2 or newer, run:
    pwsh -NoProfile -File ./verify-windows.ps1
    pwsh -NoProfile -File ./verify-windows.ps1 -Scenario demo
The verifier writes fresh evidence outside this package. Help/exit zero alone is not GUI/audio acceptance.
The official VC runtime installer is supplied but never executed automatically.
Project/resource attribution and licenses are in licenses/. Package inventory is in package.json.
Source build and complete verification instructions: docs/platforms/windows.md in the source repository.
"@
    [IO.File]::WriteAllText((Join-Path $staging 'README.md'), $readme + "`n", $encoding)
    foreach ($name in @('verify-windows.ps1', 'product-report.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $staging
    }
    # Check the actual product closure before replacing any previous package.
    foreach ($relative in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Qml.dll', 'Qt6Quick.dll',
            'Qt6Multimedia.dll', 'Qt6CanvasPainter.dll', 'platforms/qwindows.dll',
            'imageformats/qsvg.dll', 'iconengines/qsvgicon.dll', 'qml/QtQuick/qmldir',
            'qml/QtQuick/Templates/qmldir', 'qml/QtQuick/Dialogs/qmldir')) {
        if (-not (Test-Path -LiteralPath (Join-Path $staging $relative) -PathType Leaf)) {
            throw "Incomplete Windows deployment: $relative"
        }
    }
    if (-not (Get-ChildItem -LiteralPath (Join-Path $staging 'licenses/Qt') -Recurse -File)) {
        throw 'The deployed Qt license bundle is missing or empty.'
    }
    $files = @(Get-ChildItem -LiteralPath $staging -Recurse -File)
    if (-not ($files | Where-Object { $_.Directory.Name -eq 'multimedia' -and $_.Extension -eq '.dll' })) {
        throw 'The deployed Qt multimedia plugin is missing.'
    }
    if ($files | Where-Object { $_.Name -match 'Quick3D|Qt6Test|Qt6QuickTest|\.pdb$' -or
            $_.FullName -match '[/\\]qmltooling[/\\]' }) {
        throw 'The package contains development-only components.'
    }
    $dependencies = foreach ($binary in ($files | Where-Object {
                $_.Extension -in @('.dll', '.exe') -and $_.Directory.Name -ne 'prerequisites'
            })) {
        $dependencyText = & dumpbin /NOLOGO /DEPENDENTS $binary.FullName | Out-String
        if ($LASTEXITCODE -ne 0) {
            throw "Cannot inspect deployed PE dependencies: $($binary.FullName)"
        }
        $imports = [regex]::Matches($dependencyText, '(?im)^\s+([^\s]+\.dll)\s*$')
        if ($imports.Count -eq 0) {
            throw "No PE dependency evidence: $($binary.FullName)"
        }
        foreach ($import in $imports) {
            $name = $import.Groups[1].Value
            $resolution = if ((Test-Path -LiteralPath (Join-Path $staging $name)) -or
                (Test-Path -LiteralPath (Join-Path $binary.DirectoryName $name))) {
                'package'
            } elseif ($name -match '^(api-ms-|ext-ms-)') {
                'windows-api-set'
            } elseif ($name -match '^(msvcp|vcruntime|concrt)\d+.*\.dll$') {
                'official-vc-redistributable'
            } elseif ($name -notmatch '^Qt6|^(avcodec|avformat|avutil|swresample|swscale)-' -and
                (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32/$name"))) {
                'windows-system32'
            } else {
                throw "Unresolved package dependency: $($binary.Name) -> $name"
            }
            [ordered]@{
                binary = [IO.Path]::GetRelativePath($staging, $binary.FullName).Replace('\', '/')
                dependency = $name
                resolution = $resolution
            }
        }
    }
    $dependencies | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $buildDirectory 'deployment-dependencies.json') -Encoding utf8
    $inventory = foreach ($file in $files) {
        [ordered]@{
            path = [IO.Path]::GetRelativePath($staging, $file.FullName).Replace('\', '/')
            bytes = $file.Length
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $manifest = [ordered]@{
        displayName = 'yalahOO!!!'
        qtVersion = $metadata.qtVersion
        architecture = $architecture
        configuration = 'Release'
        review = $false
        compilerRuntime = 'prerequisites/' + $redist.Name
        files = @($inventory)
    }
    [IO.File]::WriteAllText((Join-Path $staging 'package.json'),
        (($manifest | ConvertTo-Json -Depth 6) + "`n"), $encoding)
    # Preserve an existing package on failure; only an explicitly owned output is replaced.
    $backup = Join-Path $repository ('out/windows-previous-' + [guid]::NewGuid().ToString('N'))
    if (Test-Path -LiteralPath $output) {
        Move-Item -LiteralPath $output -Destination $backup
    }
    try {
        Move-Item -LiteralPath $staging -Destination $output
    } catch {
        if (Test-Path -LiteralPath $backup) {
            Move-Item -LiteralPath $backup -Destination $output
        }
        throw
    }
    if (Test-Path -LiteralPath $backup) {
        Remove-Item -LiteralPath $backup -Recurse -Force
    }
    Write-Output "Windows production package: $output"
} finally {
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    Pop-Location
}
