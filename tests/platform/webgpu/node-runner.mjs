// SPDX-License-Identifier: MIT
import {webcrypto} from 'node:crypto';
import {createRequire} from 'node:module';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {runTest} from './webgpu-tests.mjs';

async function run(cli, test) {
    const moduleURL = pathToFileURL(resolve(cli));
    const {default: createConverter} = await import(moduleURL);
    if (typeof createConverter !== 'function')
        throw Error('expected a modularized img2sixel factory in ' + cli);

    // Resolve Dawn from the build tree (or NODE_PATH). A missing optional
    // package skips the case; a broken installed binding must fail it.
    let binding;
    try {
        binding = createRequire(moduleURL).resolve('webgpu');
    } catch (error) {
        if (error.code === 'MODULE_NOT_FOUND')
            return 'ok 1 # SKIP Dawn package webgpu is unavailable';
        throw error;
    }
    const {create, globals} = await import(pathToFileURL(binding));
    Object.assign(globalThis, globals);
    // Older Node releases expose Web Crypto only through node:crypto.
    globalThis.crypto ??= webcrypto;
    globalThis.navigator ??= {};
    navigator.gpu = create([]);
    return runTest(test, createConverter);
}

let result;
try {
    result = await run(process.argv[2], process.argv[3]);
} catch (error) {
    result = 'not ok 1 - ' + String(error).replace(/[\r\n]+/g, ' ');
}
// The negative CLI case sets process.exitCode to one. TAP owns the verdict;
// flush it before exiting, even if Dawn still holds native device handles.
process.stdout.write(result + '\n', () => process.exit(0));
