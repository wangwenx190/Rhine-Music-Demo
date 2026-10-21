"""Bake public-uniform shader wrappers with the selected native Qt Kit."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
TARGETS = ['SPIR-V', 'GLSL 330', 'GLSL ES 300', 'HLSL 50', 'MSL 12']
PLATFORM = 'windows' if sys.platform == 'win32' else 'macos' if sys.platform == 'darwin' else 'linux'


class ShaderBaker:
    def __init__(self, component):
        self.chain = json.loads((ROOT / 'build/release/toolchain.json').read_text(encoding='utf-8'))
        self.qsb = Path(self.chain['qtKit']) / 'bin' / ('qsb.exe' if sys.platform == 'win32' else 'qsb')
        self.work = ROOT / 'build' / (component + '-shaders')
        self.work.mkdir(parents=True, exist_ok=True)

    def bake(self, name, wrapper, *, save_dump=False):
        source = ROOT / 'resources/shaders' / name
        input_path = self.work / name
        output_path = self.work / (name + '.qsb')
        input_path.write_text(wrapper, encoding='utf-8', newline='\n')
        result = subprocess.run(
            [str(self.qsb), '--glsl', '330,300 es', '--hlsl', '50', '--msl', '12',
             '-o', str(output_path), str(input_path)], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(name + ': ' + result.stdout + result.stderr)
        dump = subprocess.run([str(self.qsb), '--dump', str(output_path)],
                              capture_output=True, text=True, check=True).stdout
        if not all(target in dump for target in ['SPIRV', 'HLSL', 'MSL', 'GLSL']):
            raise RuntimeError(name + ': missing shader variant')
        if save_dump:
            (self.work / (name + '.txt')).write_text(dump, encoding='utf-8', newline='\n')
        return dict(source=source.relative_to(ROOT).as_posix(),
                    sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    targets=TARGETS, passed=True, diagnostics=result.stderr)

    def report(self, component, value):
        output = ROOT / 'verification' / PLATFORM / component / 'shader-variants.json'
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')
