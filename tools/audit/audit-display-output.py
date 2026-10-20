#!/usr/bin/env python3
"""Bind the reviewed display-output call chains to source hashes and baked shaders."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ('COMMON-01', 'src/render/rhi/CMakeLists.txt', 'DisplayOutput.cpp',
     'Root C++23/latest-Kit selection reaches platform sources, moc and native links. '
     'App/ui-audit/test QML modules resolve VectorImage and the shared DisplaySurface; '
     'production and probe shaders use the same source. No Kit downgrade.'),
    ('COMMON-02', 'src/app/AssetCatalog.cpp', 'font.setHintingPreference',
     'Registered font faces keep physical-table matching and source identity. Quality flags '
     'are applied after face selection. SVG/qsb stay qrc-relative; no music/data paths change.'),
    ('COMMON-03', 'resources/shaders/display-output.frag', 'srgbToLinear',
     'Float item/capture/layer preserve extended sRGB; final shader unpremultiplies, decodes '
     'once, applies white scale, and repremultiplies. PQ path adds D65 gamut conversion. '
     'Production 3D shoulder is bounded by current headroom; SDR path stays source ACES.'),
    ('COMMON-04', 'src/render/rhi/DisplayOutput.cpp', 'm_window->destroy()',
     'N/A for changed media APIs: none. Surface/SG recreation retains QML/controller/audio '
     'objects; temporary persistence flags are restored. No engine/root/player replacement.'),
    ('COMMON-05', 'cmake/TestPolicy.cmake', 'display-transfer',
     'Five-pixel GPU and one small native window have separate labels. Target hardware '
     'and system notifications remain HDR-OUTPUT-P; no full/GL/performance claim.'),
    ('PERF-01', 'src/render/rhi/DisplayOutput.cpp', 'm_poll.setInterval(DisplayPollMilliseconds)',
     'Visible-only coarse query is O(displays) on Windows, O(1) for NSScreen/cached Wayland; '
     'no per-frame OS query, roundtrip, synchronous event dispatch or background decoding.'),
    ('PERF-02', 'src/platform/linux/DisplayMonitor.cpp', 'm_notifyPending',
     'Windows/Wayland notifications coalesce to one context-owned GUI callback. Linux keeps '
     'one output/description/info generation. Render confirmation queues once per epoch.'),
    ('PERF-03', 'src/render/rhi/SceneItem.cpp', 'in.hdrHeadroom == old.hdrHeadroom',
     'Only changed output values schedule a new immutable input. White/headroom change '
     'does not change target/PSO compatibility; output uniform upload remains fixed 32 bytes.'),
    ('PERF-04', 'src/render/rhi/ResourcePlan.h', 'display-composition',
     'HDR adds one whole-window FP16 layer/pass and float item/captures; capture mip payload '
     'is counted. Depth/lookup/tile isolation is retained. Swapchain/driver costs stay unknown. '
     'No extra whole-window MSAA or product readback.'),
    ('PERF-05', 'src/render/rhi/RhiItem.cpp', 'plannedHdr',
     'Descriptor plan invalidates on HDR/size; shared geometry/cover/postprocess caches retain '
     'their original keys. QML layers are conditional, not a second scene renderer.'),
    ('PERF-06', 'qml/Yalahoo/Music/components/DisplaySurface.qml', 'property DisplayOutput output',
     'Typed GUI output state owns policy; QML holds visual bindings only. No library-sized '
     'loop or second business/config owner. Item content and interactions remain in place.'),
    ('PERF-07', 'src/render/rhi/DisplayOutput.cpp', 'epoch != m_epoch.load()',
     'Window outlives render callbacks, output outlives engine. Mode rebuild joins old render '
     'work before native destroy; queued stale confirmations are rejected. Native observers/ '
     'proxies/filter and timer are released by their owner; hide/minimize stops polling.'),
]
PLATFORMS = {
    'windows': ('WIN-01', 'src/platform/windows/DisplayMonitor.cpp', 'DISPLAYCONFIG_SDR_WHITE_LEVEL',
                'MonitorFromWindow selects output; fresh DXGI factory/desc proves current PQ HDR, '
                'not WCG. QueryDisplayConfig retries topology races at most three times; white '
                'query failure is SDR. WM changes queue refresh outside Win32 dispatch.'),
    'macos': ('MAC-01', 'src/platform/macos/DisplayMonitor.mm', 'maximumExtendedDynamicRangeColorComponentValue',
              'Qt screen maps to NSScreen. Potential EDR gates support; current headroom limits '
              'rendering while 1.0 remains system-relative SDR white. Main-queue observer plus '
              'coarse query covers screen/brightness; observer is removed before owner dies.'),
    'linux': ('LNX-01', 'src/platform/linux/DisplayMonitor.cpp', 'WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ',
              'Qt owns display dispatch/color surface. Async output description validates transfer, '
              'target peak/reference and advertised parametric encoding. PQ is preferred, linear '
              'is supported; missing protocol/unknown/SDR description returns SDR. Default Vulkan '
              'initialization/required formats are probed once; explicit GL remains available.'),
}


def main():
    metadata = json.loads((ROOT / 'build/release/toolchain.json').read_text())
    qsb = Path(metadata['qtKit']) / 'bin' / ('qsb.exe' if os.name == 'nt' else 'qsb')
    baked = {}
    for name, path in (
        ('display', 'build/release/src/app/.qsb/display-output.frag.qsb'),
        ('scene', 'build/release/src/render/rhi/.qsb/output.frag.qsb'),
    ):
        dump = subprocess.run([str(qsb), '-d', str(ROOT / path)], check=True,
                              capture_output=True, text=True).stdout
        if not all(kind in dump for kind in ('SPIRV', 'GLSL', 'HLSL', 'MSL')):
            raise SystemExit(f'Missing shader variant: {name}')
        baked[name] = dict(path=path,
                          sha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest(),
                          variants=dump)
    checks = []
    for platform, specific in PLATFORMS.items():
        for check_id, path, symbol, evidence in [*CHECKS, specific]:
            data = (ROOT / path).read_bytes()
            source = data.decode()
            if symbol not in source:
                raise SystemExit(f'Review binding changed: {path}::{symbol}')
            checks.append(dict(checkId=check_id, requirement=check_id, platform=platform,
                sourcePath=path, lineOrSymbol=symbol, sourceSha256=hashlib.sha256(data).hexdigest(),
                evidence=evidence, result='source-reviewed',
                limitations='Target compilation/native HDR hardware/physical events are separate; see HDR-OUTPUT-P.'))
    output = ROOT / 'verification/display-output-static/source-matrix.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(dict(checks=checks, bakedShaders=baked,
        targetExecution='Not established by source audit'), indent=2) + '\n')
    print('Display output call chains and cross-target shader variants reviewed; runtime evidence is separate')


if __name__ == '__main__':
    main()
