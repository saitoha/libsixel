import { readFileSync } from 'node:fs';
import { argv, cwd } from 'node:process';
import { WASI } from 'node:wasi';
import './emscripten-node-stdio.js';

const args = argv.slice(2);
const wasi = new WASI({
  version: 'preview1',
  args,
  preopens: { '.': cwd() },
  returnOnExit: true,
});
const module = new WebAssembly.Module(readFileSync(args[0]));
const instance = new WebAssembly.Instance(module, wasi.getImportObject());
process.exitCode = wasi.start(instance);
