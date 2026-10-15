[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
throw 'Historical R10A image verification is retired. Build Release with YALAHOO_REVIEW=ON, then run ctest --preset qt-release-visible -R ^rhi-opaque-gpu$ (macOS hidden: qt-release-render). See docs/contracts/rhi-opaque.md.'
