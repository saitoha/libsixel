# Local PoC Workflow for Fuzz Investigation

This workflow keeps third-party PoCs out of git history and commits only
regenerated inputs derived from root-cause analysis.

## Policy

- Keep external PoCs under `.tmp/` only.
- Never `git add` downloaded third-party inputs.
- Commit only regenerated PoCs that you created yourself.
- Always verify regenerated PoCs hit the same crash signature before commit.

## 1. Prepare external PoCs locally

Example datasets discussed for local-only intake:

- https://github.com/dvyukov/go-fuzz-corpus/tree/master/png/corpus
- https://github.com/dvyukov/go-fuzz-corpus/tree/master/jpeg/corpus
- https://github.com/dvyukov/go-fuzz-corpus/tree/master/gif/corpus

Example command:

```bash
set -euxo pipefail
mkdir -p .tmp/external-corpus
rm -rf .tmp/external-corpus/go-fuzz-corpus-src

git clone --depth 1 https://github.com/dvyukov/go-fuzz-corpus \
  .tmp/external-corpus/go-fuzz-corpus-src

mkdir -p .tmp/external-corpus/go-fuzz/{png,jpeg,gif}
cp -a .tmp/external-corpus/go-fuzz-corpus-src/png/corpus/. \
  .tmp/external-corpus/go-fuzz/png/
cp -a .tmp/external-corpus/go-fuzz-corpus-src/jpeg/corpus/. \
  .tmp/external-corpus/go-fuzz/jpeg/
cp -a .tmp/external-corpus/go-fuzz-corpus-src/gif/corpus/. \
  .tmp/external-corpus/go-fuzz/gif/
```

## 2. Build local runner

Build whichever runner you want to triage against.

Example (autotools, CLI parser target):

```bash
set -euxo pipefail
CC=afl-clang-fast CXX=afl-clang-fast++ \
  CFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' \
  LDFLAGS='-fsanitize=address,undefined' \
  ./configure

make -j"$(getconf _NPROCESSORS_ONLN)" fuzz-cli-img2sixel-parser
```

## 3. Triage external PoCs locally

Use the local triage helper to produce JSON reports and stack-hash clustering.

```bash
set -euxo pipefail
tools/fuzz-local-triage.sh \
  --input-dir .tmp/external-corpus/go-fuzz/png \
  --asan-runner .tmp/fuzz-img2sixel-cli-parser-afl-persistent \
  --triage-dir .tmp/local-poc-triage/png \
  --label go-fuzz-png-parser
```

Outputs:

- `.tmp/local-poc-triage/.../summary.tsv`
- `.tmp/local-poc-triage/.../summary-by-stack.tsv`
- per-input triage JSON files from `.github/scripts/fuzz/triage_afl_crash.sh`

## 4. Root-cause and regenerate

- Analyze the crash root cause in code.
- Create a clean regenerated PoC from your own understanding of the bug.
- Store regenerated PoCs under repository-managed test input locations.

## 5. Verify regenerated PoC matches crash signature

Compare regenerated candidate with baseline triage JSON.

```bash
set -euxo pipefail
tools/fuzz-verify-regenerated-poc.sh \
  --baseline-json .tmp/local-poc-triage/png/go-fuzz-png-parser-id_000001.json \
  --candidate tests/data/security/fuzzing/data/regenerated-parser-crash-001.png \
  --asan-runner .tmp/fuzz-img2sixel-cli-parser-afl-persistent
```

A `PASS` means:

- candidate reproduces under ASAN, and
- stack hash matches baseline.

## 6. Commit scope

Commit only:

- source fix
- regenerated PoC
- regression test

Do not commit `.tmp/external-corpus` or any third-party raw PoC blobs.
