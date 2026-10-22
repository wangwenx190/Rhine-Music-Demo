// Explicit source-audit regression; ordinary native CTest does not depend on Node.
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {validateTitleState} from '../../tools/migration/title-report.mjs';

const options = {
    backend: 'metal',
    dpr: 1.5,
    visible: false,
    platform: 'darwin'
};
function report() {
    return {
        passed: true,
        backend: 'metal',
        dpr: 1.5,
        released: true,
        windowPolicy: {passed: true},
        motionError: 0,
        offscreen: true,
        renderProtocol: 'render-control-gui-thread',
        captures: Array.from(
            {length: 20},
            (_, index) =>
                ({case: index, valid: true, geometry: {passed: true}, structurePassed: true})),
        headerChecks: Array.from({length: 18}, () => ({passed: true})),
        lifecycle: [{passed: true}],
        animationChecks: [{passed: true, glyphNodesReused: 1}]
    };
}
test('complete title state and native window scopes pass', () => {
    assert.equal(validateTitleState(report(), options).length, 20);
    const native = {...report(), offscreen: false, renderProtocol: 'native-window'};
    assert.equal(validateTitleState(native, {...options, visible: true}).length, 20);
});
test('truthy flags, missing/nonfinite numbers and repeated cases fail', () => {
    for (const change of [{passed: 'true'}, {released: 1}, {windowPolicy: {passed: 'false'}},
                          {motionError: undefined}, {motionError: NaN}, {motionError: Infinity},
                          {motionError: -.1}, {dpr: '1.5'}, {dpr: NaN}, {offscreen: 1}])
        assert.throws(() => validateTitleState({...report(), ...change}, options));
    for (const section of ['captures', 'headerChecks', 'lifecycle', 'animationChecks']) {
        const value = report();
        value[section][0].passed = 'true';
        if (section === 'captures')
            value.captures[0].valid = 'true';
        assert.throws(() => validateTitleState(value, options));
    }
    const repeated = report();
    repeated.captures[1].case = repeated.captures[0].case;
    assert.throws(() => validateTitleState(repeated, options));
    for (const reused of [undefined, NaN, 0, '1']) {
        const value = report();
        value.animationChecks[0].glyphNodesReused = reused;
        assert.throws(() => validateTitleState(value, options));
    }
});
