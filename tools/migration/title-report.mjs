// Validate native title assertions independently from CLI transport and source font comparison.
export function validateTitleState(report, {backend, dpr, visible, platform}) {
    const production = report.captures;
    if (report.backend !== backend || !Number.isFinite(report.dpr)
        || Math.abs(report.dpr - dpr) > .001 || report.released !== true
        || report.windowPolicy?.passed !== true || !Number.isFinite(report.motionError)
        || report.motionError < 0 || report.motionError > .000001 || !Array.isArray(production)
        || production.length !== 20 || !Array.isArray(report.headerChecks)
        || report.headerChecks.length !== 18 || !Array.isArray(report.lifecycle)
        || report.lifecycle.length !== 1 || !Array.isArray(report.animationChecks)
        || report.animationChecks.length !== 1 || report.passed !== true)
        throw Error('Backend, DPR, motion, window, capture or lifecycle gates failed');
    if (visible && (report.offscreen !== false || report.renderProtocol !== 'native-window'))
        throw Error('Visible title verification requires a native window');
    if (!visible && platform === 'darwin'
        && (report.offscreen !== true || report.renderProtocol !== 'render-control-gui-thread'))
        throw Error('macOS title verification requires a hidden RenderControl target');
    if (production.some(capture => capture.valid !== true || capture.geometry?.passed !== true
                                   || capture.structurePassed !== true)
        || report.headerChecks.some(check => check.passed !== true)
        || report.lifecycle.some(check => check.passed !== true))
        throw Error('Structure, header or lifecycle gates failed');
    const cases = production.map(capture => capture.case);
    if (cases.some(value => !Number.isInteger(value) || value < 0 || value >= production.length)
        || new Set(cases).size !== production.length)
        throw Error('Exactly one native capture per title case required');
    if (report.animationChecks.some(sample => sample.passed !== true
                                              || !Number.isInteger(sample.glyphNodesReused)
                                              || sample.glyphNodesReused <= 0))
        throw Error('Production glyph reuse failed');
    return production;
}
