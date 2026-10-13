param(
    [string[]]$Backends = @('d3d11', 'opengl'),
    [double[]]$Dprs = @(1, 1.5, 2)
)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $repository 'verification/windows/browse-ui'
$webRoot = Join-Path $evidence 'web'
$web = Get-Content -Raw (Join-Path $webRoot 'report.json') | ConvertFrom-Json
$utf8 = [Text.UTF8Encoding]::new($false)
$records = @()
$passed = $true
foreach ($backend in $Backends) {
    foreach ($dpr in $Dprs) {
        $directory = Join-Path $evidence "$backend-dpr-$dpr"
        $native = Get-Content -Raw (Join-Path $directory 'report.json') | ConvertFrom-Json
        foreach ($sample in $native.browse) {
            $expected = @($web.captures | Where-Object { $_.width -eq $sample.width -and $_.height -eq $sample.height -and $_.night -eq $sample.night -and $_.dpr -eq $dpr })
            if ($expected.Count -ne 1) {
                throw 'A unique same-input frozen UI capture is required'
            }
            $expected = $expected[0]
            $anchors = @{}
            foreach ($name in @('header', 'brand', 'browse', 'title', 'play', 'stop')) {
                $properties = if ($name -eq 'brand') {
                    @('x', 'y')
                } else {
                    @('x', 'y', 'width')
                }
                foreach ($property in $properties) {
                    $delta = [Math]::Abs($sample.$name.$property - $expected.$name.$property)
                    $anchors["$name.$property"] = $delta
                    if ($delta -gt 1) {
                        $passed = $false
                    }
                }
            }
            $rois = @{
                browse = @($expected.browse.x, $expected.browse.y, $expected.browse.width, $expected.browse.height)
                transport = @($expected.nav.x, ($expected.nav.y - 1), $expected.nav.width, ($expected.nav.height + 2))
                footer = @($expected.footer.x, ($expected.footer.y - 2), $expected.footer.width, ($expected.footer.height + 4))
            }
            $metrics = @{}
            foreach ($name in $rois.Keys) {
                $rect = $rois[$name]
                $x = $rect[0] * $dpr
                $y = $rect[1] * $dpr
                $right = ($rect[0] + $rect[2]) * $dpr
                $bottom = ($rect[1] + $rect[3]) * $dpr
                $polygon = @(@($x, $y), @($right, $y), @($right, $bottom), @($x, $bottom))
                $stem = Join-Path $directory "$($sample.file)-$name"
                $opaqueAlpha = '255'
                $maskFile = if ($name -eq 'transport') {
                    $expected.transportOutline
                } else {
                    $expected.alphaFile
                }
                $maskScale = if ($name -eq 'transport') {
                    '8'
                } else {
                    '1'
                }
                [IO.File]::WriteAllText("$stem-roi.json", ($polygon | ConvertTo-Json -Depth 3), $utf8)
                & (Join-Path $repository 'build/release/tools/compare-images.exe') (Join-Path $directory $sample.file) (Join-Path $webRoot $expected.file) "$stem-roi.json" "$stem-metrics.json" (Join-Path $directory "$($sample.file)-diff.png") (Join-Path $webRoot $maskFile) $opaqueAlpha $maskScale
                if ($LASTEXITCODE -ne 0) {
                    throw 'Same-input image comparison failed'
                }
                $comparison = Get-Content -Raw "$stem-metrics.json" | ConvertFrom-Json
                $metrics[$name] = $comparison
                if ($null -eq $comparison.nonEdge.ssim -or $comparison.nonEdge.ssim -lt 0.98 -or $comparison.nonEdge.nonBandMeanAbsoluteRgb -gt 6 -or $null -eq $comparison.edgeDistancePixels -or $comparison.edgeDistancePixels -gt 1) {
                    $passed = $false
                }
            }
            $records += @{
                backend = $backend
                width = $sample.width
                height = $sample.height
                dpr = $dpr
                night = $sample.night
                anchors = $anchors
                regions = $metrics
            }
        }
    }
}
$reportName = if ($Backends.Count -eq 2 -and $Dprs.Count -eq 3) {
    'paired-ui.json'
} else {
    'paired-ui-subset.json'
}
[IO.File]::WriteAllText((Join-Path $evidence $reportName), (@{
            passed = $passed
            baseline = $web.baseline
            sources = $web.sources
            excludedBrand = 'Only brand graphics differ; header container anchors are compared. Pending R15 navigation is outside this R14 UI capture.'
            records = $records
        } | ConvertTo-Json -Depth 12), $utf8)
if (-not $passed) {
    throw 'Paired UI structure/image gates failed; metrics retained'
}
Write-Output 'Paired R14 UI passed'
