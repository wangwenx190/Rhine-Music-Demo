from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
for shader,parts in [('album-paper.frag',['lighting-field','lighting-environment']),('case-glass.frag',['lighting-field','lighting-ltc','lighting-direct'])]:
    path=root/'resources/shaders'/shader
    text=path.read_text(encoding='utf-8')
    begin='// BEGIN SHARED LIGHTING\n';end='// END SHARED LIGHTING\n'
    body=begin+''.join((root/'resources/shaders'/f'{p}.glsl').read_text(encoding='utf-8')+'\n' for p in parts)+end
    if begin in text:
        a=text.index(begin);b=text.index(end,a)+len(end);result=text[:a]+body+text[b:]
    else: result=body+text
    if '--check' in sys.argv:
        if result!=text:raise SystemExit(f'Stale shared lighting: {shader}')
    else:path.write_text(result,encoding='utf-8',newline='\n')
