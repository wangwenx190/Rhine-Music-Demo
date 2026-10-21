# Shared console command logging and process-environment restoration for Review wrappers.
function Invoke-ReviewCommand {
    param([string]$Name, [scriptblock]$Command, [string]$OutputDirectory)
    $output = & $Command 2>&1
    $code = $LASTEXITCODE
    [IO.File]::WriteAllText((Join-Path $OutputDirectory $Name), ($output | Out-String), [Text.UTF8Encoding]::new($false))
    if ($code -ne 0) {
        throw "$Name failed; output retained"
    }
    Write-Output "$Name passed"
}

function Save-ProcessEnvironment {
    param([string[]]$Names)
    $saved = @{}
    foreach ($name in $Names) {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    }
    return $saved
}

function Restore-ProcessEnvironment {
    param([hashtable]$Saved)
    foreach ($name in $Saved.Keys) {
        [Environment]::SetEnvironmentVariable($name, $Saved[$name], 'Process')
    }
}

function Initialize-ReviewBuild {
    param([string]$OutputDirectory)
    . (Join-Path $PSScriptRoot 'enter-msvc.ps1')
    Invoke-ReviewCommand 'configure.txt' { cmake --preset qt-release -DYALAHOO_REVIEW=ON } $OutputDirectory
    Invoke-ReviewCommand 'format.txt' { cmake --build --preset qt-release --target format } $OutputDirectory
    Invoke-ReviewCommand 'check-format.txt' { cmake --build --preset qt-release --target check-format } $OutputDirectory
    Invoke-ReviewCommand 'build.txt' { cmake --build --preset qt-release } $OutputDirectory
    Invoke-ReviewCommand 'qmllint.txt' { cmake --build --preset qt-release --target Yalahoo_qmllint ui-audit_qmllint } $OutputDirectory
    $chain = Get-Content -Raw build/release/toolchain.json | ConvertFrom-Json
    $env:PATH = "$($chain.qtKit)/bin;$env:PATH"
    $env:QT_PLUGIN_PATH = "$($chain.qtKit)/plugins"
    $env:QSG_RENDER_LOOP = 'threaded'
    Remove-Item Env:QSG_NO_VSYNC -ErrorAction SilentlyContinue
    $env:QT_SCALE_FACTOR = '1'
}
