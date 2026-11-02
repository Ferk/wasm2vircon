// npm owns the frontend build; Node's filesystem/process APIs keep it portable.
import asc from 'assemblyscript/asc';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = dirname(fileURLToPath(import.meta.url));
const project = 'as-boids';
const entry = 'vircon_main';
let output = resolve(root, 'build');
let wasmOnly = false;
for (let i = 2; i < process.argv.length; ++i) {
  const arg = process.argv[i];
  if (arg === '--wasm-only') wasmOnly = true;
  else if (arg === '--out-dir' && process.argv[i + 1]) output = resolve(process.argv[++i]);
  else throw new Error(`Unknown or incomplete argument: ${arg}`);
}
mkdirSync(output, { recursive: true });
const artifact = (extension) => resolve(output, `${project}.${extension}`);
const result = await asc.main([
  '--baseDir', root, '--config', resolve(root, 'asconfig.json'),
  '--target', 'release', '--outFile', artifact('wasm'),
], { stdout: process.stdout, stderr: process.stderr });
if (result.error) process.exit(1);
if (!wasmOnly) {
  const run = (tool, args) => {
    const child = spawnSync(tool, args, { stdio: 'inherit', cwd: root });
    if (child.error) throw new Error(`Cannot run ${tool}: ${child.error.message}`);
    if (child.status !== 0) process.exit(child.status ?? 1);
  };
  run(process.env.WASM2VIRCON_EXECUTABLE || 'wasm2vircon',
    [artifact('wasm'), '--entry', entry, '-o', artifact('asm')]);
  run(process.env.ASSEMBLE_EXECUTABLE || 'assemble', ['-o', artifact('vbin'), artifact('asm')]);
  writeFileSync(artifact('rom.xml'), `<?xml version="1.0" encoding="UTF-8"?>
<rom-definition version="1.0">
  <rom type="cartridge" title="AssemblyScript Boids" version="1.0" />
  <binary path="${project}.vbin" />
  <textures />
  <sounds />
</rom-definition>
`);
  run(process.env.PACKROM_EXECUTABLE || 'packrom', ['-o', artifact('v32'), artifact('rom.xml')]);
}
console.log(`Built ${artifact(wasmOnly ? 'wasm' : 'v32')}`);
