# Contributing to libsixel

Thank you for considering a contribution to libsixel. The project includes a
SIXEL encoder and decoder, command-line tools, image loaders, language
bindings, desktop integration, fuzzing targets, quality assessment tools, and
multi-platform CI. It also maintains both Autotools and Meson as first-class
build systems. Small bug fixes, tests, documentation, portability fixes, and
image-quality improvements are all welcome.

## Before You Start

Before making a change, check the related issues, pull requests, `README.md`,
`build.md`, `SECURITY.md`, and nearby tests. In libsixel, user-visible behavior,
CLI help, man pages, README text, build options, shell completion, bindings,
TAP regression tests, and staticcheck contracts are expected to stay in sync.

In this repository, staticcheck means libsixel's own lint/check suite. It is
run before or alongside regression tests to catch drift between contracts
derived from IDL, annotations, and generated registries and the corresponding
code, documentation, and tests.

If you believe you have found a security issue, follow `SECURITY.md` and use
GitHub Private Vulnerability Reporting instead of opening a public issue.
Please do not report potential memory corruption, arbitrary code execution, or
information disclosure from out-of-bounds reads in public issues.

## Reporting Issues

Please include enough information for maintainers to reproduce the problem:

- the libsixel version or commit hash
- OS, CPU architecture, compiler, and build system
- Autotools or Meson configuration options
- the exact command line
- actual result and expected result
- crash log, sanitizer log, or stack trace
- a minimized input file when possible

For image inputs and fuzzing proof-of-concepts, keep the reproducer as small as
possible. Avoid submitting copyrighted third-party content directly; prefer
synthetic or minimized data made specifically for the bug.

## Pull Requests

Keep pull requests small enough that their intent is clear. If one change fixes
several distinct failure classes, split the commits when practical.

A good pull request explains:

- what changed
- why the change is needed
- how it was verified
- whether GitHub Actions CI is passing, still running, or failing
- which environments or options were not verified
- whether CLI behavior, public API, output format, or diagnostics changed

Use short, specific commit messages. Prefixes such as `fix:`, `test:`,
`docs:`, `ci:`, and `tools:` make the change type easier to scan.

## Development Environment

This repository maintains both Autotools and Meson. Neither is merely a
fallback; both are supported user-facing build paths.

This setup is not required just to build or test the project. Contributors who
modify or regenerate build-system files, commit generated build outputs, or
update the build tools themselves should use the libsixel-standard Autotools
and Meson versions. This avoids noisy generated Autotools diffs and prevents
Meson files from accidentally using syntax that the supported Meson version
does not accept. Those tools are installed under `$TOP_SRCDIR/.local/bin` by
the setup script. From the source tree root:

```sh
TOP_SRCDIR=${TOP_SRCDIR:-$PWD}
"$TOP_SRCDIR/tools/setup-buildtools.bash"
export PATH="$TOP_SRCDIR/.local/bin:$PATH"
```

For contributors, installing the optional staticcheck tools is strongly
recommended. Missing tools usually make the corresponding check skip rather
than fail, but a full pre-PR staticcheck run is more useful when these tools
are available:

- `actionlint` for GitHub Actions workflow linting
- `shellcheck` for shell TAP and helper scripts
- `codespell` for typo checks
- `python3` for Python syntax checks and Python-based static checks
- Python `tree-sitter` and `tree-sitter-c` bindings for C AST contract checks

Install the Python bindings for the same interpreter that Autotools or Meson
will detect. For example:

```sh
python3 -m pip install tree-sitter tree-sitter-c
```

On macOS with Homebrew, the standalone tools can usually be installed with:

```sh
brew install actionlint shellcheck codespell
```

End users do not need these tools; they are only recommended for contributors
who run the full staticcheck suite locally.

Basic Autotools verification:

```sh
./configure --enable-tests
make staticcheck
make
make check
```

Autotools builds in this repository enable parallelism by default, so `-j` is
not normally needed. staticcheck does not require built binaries, so run it
before the build and regression test steps.

Basic Meson verification:

```sh
meson setup builddir
meson compile -C builddir staticcheck
meson compile -C builddir
meson test -C builddir
```

For narrow investigations, it is fine to begin with the smallest build, test,
or staticcheck case that reproduces the behavior. For pull requests, however,
run the full local staticcheck and regression test suite unless there is a
clear reason not to.

## Local Checks and CI

libsixel has many combinations of build options, optional dependencies,
compilers, C libraries, platforms, sanitizers, fuzzing modes, Autotools, and
Meson. It is not realistic to verify every combination on a contributor's
local machine. The expected local balance is to run the full staticcheck suite
and regression tests as a smoke check. This normally takes only a few minutes,
so the risk of missing regressions by narrowing the local test scope is usually
larger than the saved time.

Use GitHub Actions CI for the wider matrix: pcc/tcc, MSVC/clang-cl, MinGW,
MSYS2/Cygwin, musl, Cosmopolitan libc, BSD/Solaris/Haiku, Emscripten,
sanitizers, fuzzing, amalgamation, and Meson unity builds. In your pull request,
include the local commands you ran and the GitHub Actions CI status, including
any failing or still-running jobs.

## Autotools and Meson Dual Maintenance

In libsixel, Autotools/Meson drift is not something to leave for someone else.
It is a compatibility surface that the contributor should check. Updating only
one build system can break another platform, packager, CI job, or downstream
user.

Check both Autotools and Meson when changing:

- added or removed source files, headers, generated sources, or amalgamation
  inputs
- libraries, CLI tools, bindings, assessment tools, or fuzz targets
- optional dependencies, feature options, compiler probes, or linker probes
- install targets, installcheck behavior, completion, or desktop integration
- sanitizer, analyzer, coverage, fuzzing, or cross-build configuration
- test runners, environments, timeouts, or skip conditions under `tests/`

Common files to update:

- `Makefile.am` and the corresponding `Makefile.in`
- `configure.ac`, `m4/`, and `build-aux/`
- `meson.build`, subdirectory `meson.build` files, and `meson_options.txt`
- Autotools/Meson option tables in `build.md`
- `.github/workflows/` matrices and platform-specific commands
- `tests/Makefile.am` and `tests/meson.build`
- sync checks under `tests/_static/sh/`

If a behavior can only be represented in one build system, explain why in the
pull request. The default policy is to keep the same feature intent, test
intent, and install intent in both build systems.

## New Source Files, Unity Builds, and Amalgamation

When adding a new `.c`, `.h`, `.m`, or test source file, consider normal builds,
Meson unity builds, and amalgamation builds. Code that is separate in a normal
build can become part of the same translation unit in unity or amalgamation
mode, so generic `static` helper names, file-scope macros, and include-order
side effects can become collisions or build failures.

When adding a file, check:

- Autotools `Makefile.am` and generated `Makefile.in`
- the relevant Meson `meson.build`
- `amalgamation/meson.build` `amalgamation_units` and `amalgamation_headers`
- whether `amalgamation/Makefile.am` dependency discovery covers it
- whether test registration is needed in `tests/Makefile.am` and
  `tests/meson.build`
- whether source-list or amalgamation sync checks under `tests/_static/sh/`
  need updates

The amalgamated `sixel.c` is generated by `tools/gen-amalgamation.sh`. The
script collects header and source units from areas such as `include`, `src`,
`converters`, `tests`, and `assessment`, concatenates headers first, and
removes project-local includes so the result can compile as one translation
unit. `BUILD_*` compile guards decide which library, converter, assessment
tool, or test-runner entry points are materialized. Do not edit generated
`amalgamation/sixel.c` directly.

Meson amalgamation uses explicit lists in `amalgamation/meson.build`. When you
add production sources or private headers, check whether they also belong in
`amalgamation_units` or `amalgamation_headers`. Autotools dependency discovery
is more wildcard-based for regenerating the amalgamated file, but normal
library source lists and distribution lists still need to stay synchronized via
`Makefile.am` and `Makefile.in`.

To keep unity and amalgamation builds healthy:

- give `static` helpers module-specific names rather than generic names such
  as `read_file` or `parse_header`
- keep file-local macros minimal, and `#undef` macros that would be dangerous
  if they leaked into later units
- guard platform-specific includes and API calls with configure/Meson probes
  and feature macros
- align tool-only entry points with the existing `BUILD_*` guards
- include both generated files and their sources in source lists or dependency
  lists when generated headers or registries are involved

## C Style

Match the style of nearby code. C code is C99 with the project's K&R-like local
style.

- Write comments in English.
- Keep C source lines within 80 columns where practical.
- Match nearby indentation, brace style, and spacing.
- Declare local variables at the start of the function scope.
- Follow existing include ordering for `config.h` and private headers.
- Do not expose unnecessary helpers or internal types from public headers.
- Read nearby comments before editing, and update stale comments while you are
  there.

libsixel considers old compilers, uncommon compilers such as pcc and tcc, MSVC
toolchains, non-Linux environments such as BSD and Solaris, MSYS2/Cygwin-like
environments, musl and Cosmopolitan libc, Emscripten, Meson unity builds, and
amalgamation builds. Avoid compiler-specific conveniences unless they are
guarded by configure/Meson probes.

## Generated Headers and Registries

Some tracked files are generated headers or generated registries. Do not edit
generated outputs directly. Doing so breaks the contract with the source file
and usually gets reverted by the next regeneration or caught by staticcheck.
Change the source and regenerate with the existing generator instead.

Representative examples:

- `include/6cells.h` is generated from `include/6cells.idl` by
  `tools/gen_6cells_h.awk`.
- The component factory registry is generated from class-id annotations by
  `tools/gen_factory_classid_gperf.awk`, which creates
  `src/classid-factory.gperf`; `gperf` then generates
  `src/classid-factory.h`.
- The service registry is generated from service-id annotations by
  `tools/gen_serviceid_gperf.awk`, which creates
  `src/classid-service.gperf`; `gperf` then generates
  `src/classid-service.h`.
- `src/lso2.h` is generated from `src/lso2.key` by
  `tools/gen_varcoefs.awk`.
- `src/rgblookup.h` is generated from `src/rgblookup.gperf` by `gperf`.

When changing IDL, annotations, `.key`, or `.gperf` inputs, update the
corresponding generated outputs and verify that Autotools/Meson source lists,
amalgamation inputs, and staticcheck sync checks all pass.

## Documentation and User-Facing Contracts

When changing CLI options, suboptions, environment variables, build options, or
public API, update the matching user-facing contracts as well.

Common places to check:

- `README.md`
- `build.md`
- CLI help text
- man page sources
- shell completion
- `include/sixel.h.in`
- language bindings
- sync checks under `tests/_static/sh/`

Help text and README wording are often tested contracts, not cosmetic prose.
Do not update only the implementation while leaving help, docs, completion, or
staticcheck behind.

## Autotools Generated Files

When changing `configure.ac`, `Makefile.am`, `m4/`, or other Autotools
metadata, update the required generated files. `configure`, `Makefile.in`, and
related `.in` files may be tracked outputs.

If you are unsure whether a generated output belongs in the pull request,
mention it explicitly. Do not include build artifacts, local experiment files,
coverage output, or fuzzing output.

## Adding Tests

Bug fixes should include regression tests when practical. For crashes and
fuzzing findings, the preferred flow is:

1. Reproduce the issue on the current tree.
2. Minimize the proof-of-concept.
3. Add a regression test that fails before the fix.
4. Fix the implementation.
5. Re-run the added test and the related suite.

Place tests under the appropriate `tests/` category and follow the existing
serial numbering and naming scheme. Maintain numbering when it drifts. If the
purpose of a test changes, rename the test file so the filename still matches
the behavior being checked. See `tests/guildline.md` for more detail about
input size and shell TAP style.

## Shell TAP Test Style

Shell `.t` tests are optimized for speed and deterministic diagnostics.

- Start with `set -eux`.
- You may temporarily use `set +e` when needed, but keep the initial form
  `set -eux`.
- Enable `set -v` immediately after printing the TAP plan.
- Use `1..1` by default. Use `1..0 # SKIP ...` only for skip cases.
- Keep one test case per file.
- Make the test name and filename match the behavior being tested.
- Keep control flow flat and readable from top to bottom.
- Do not define helper functions.
- Avoid unnecessary variables, temporary files, and output.
- Avoid `if`, `elif`, `else`, and `case` when a linear fail/pass flow is
  sufficient.
- Do not nest `|| { ... }` blocks.
- Use `test ...`, not `[ ... ]`.
- Report expectation failures with TAP `not ok` and `exit 0`; do not signal
  test failure via process exit status.
- Use `${0%/*}` and `${0##*/}` instead of `dirname` and `basename`.
- Avoid forks, subshells, and multi-stage pipelines.
- Do not call Python from shell tests.
- Do not use `grep`, `awk`, or `sed` in shell TAP tests because they add forks.
- Prefer shell parameter expansion and pattern matching for string checks.
- Do not use `cp` or `rm`.
- Use `mkdir` only when a test directory really needs to be created.
- When writing under `ARTIFACT_LOCAL_DIR`, create that directory with
  `mkdir -p`.
- Send unverified output to `/dev/null`, not to a file.
- Do not create output files or diagnostic log files unless the test asserts
  their content.
- Do not write redirects to empty paths such as `>""` or `2>""`.

The typical shape is to emit TAP `fail` and exit successfully when a command or
condition fails, then emit `pass` at the end:

```sh
command1 ... || {
    fail 1 "..."
    exit 0
}
command2 ... || {
    fail 1 "..."
    exit 0
}
test condition ... || {
    fail 1 "..."
    exit 0
}
pass 1 "..."
exit 0
```

These constraints keep tests practical on environments where fork cost is high,
especially Windows. staticcheck catches `grep`/`awk`/`sed`, multiple TAP plans,
missing `ARTIFACT_LOCAL_DIR` setup, `if`, shell functions, nested `|| { ... }`,
`[ ... ]`, empty redirects, and similar shell-test policy violations.

Run shellcheck on changed shell tests and make sure it emits no errors,
warnings, or info messages:

```sh
ARTIFACT_LOCAL_DIR=$PWD TOP_SRCDIR=$PWD \
find tests -type f -name '*.t' -exec shellcheck -x -P "$PWD" {} +
```

C tests and implementation code also have staticcheck-enforced contracts. In C
tests, avoid direct `getenv()` and `fopen()` calls because they can behave
differently across CRTs and platforms; use the existing test helpers or
wrappers. In `src/*.c`, include `config.h` first and use existing environment
helper APIs instead of calling `getenv()` directly.

## Image Quality Tests

LSQA stands for libsixel quality assessment. It is a libsixel-specific tool for
evaluating image quality. In the past, command success and local pixel checks
were not enough to catch regressions where dithering, quantization, color
management, or loader fallback visibly collapsed image quality. LSQA exists to
detect quality and perceptual regressions in tests.

LSQA reads an expected image as `<reference>` and the output under test as
`[target]`. It loads both through libsixel loaders, normalizes them to the same
colorspace and precision, and compares them. `target` may be a normal image
such as PNG/JPEG/PNM or SIXEL output generated by `img2sixel`. Use `-` as the
target to read from standard input.

The common pattern is a baseline check that fails when MS-SSIM drops below a
floor:

```sh
${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" \
    "${reference_image}" "${output_sixel}" >/dev/null 2>&1
```

Use `-m MS-SSIM` to print only one metric. `-b METRIC:VALUE` returns exit code
5 when the metric is below the baseline, so TAP tests can distinguish quality
regressions from runtime failures. `-W` selects comparison colorspace, `-P`
selects comparison precision, and `-g` compares grayscale luminance. Use
`-d k_undither` when a SIXEL target should be dequantized before comparison.

Keep LSQA inputs as small as the behavior allows:

- Use about 16x16 for basic loader corruption checks.
- Use about 64x64 for dithering or quantization quality checks.
- Use 2x2 to 8x8 for exact tone or color-management checks.
- Use 1x1 or 1x2 only for placeholder pass/fail checks that are not about
  visual quality.

When using a fixture derived from `images/snake.png`, avoid making it larger
than necessary. For alpha or transparency tests, use ImageMagick for the
smallest needed transformation. Do not normally pass a palette-size option
(`-p`); use the default 256 colors. Use MS-SSIM 0.98 as the normal quality
baseline, and do not lower a threshold below 0.95 without a very strong reason.

## Fuzzing and Sanitizers

For fuzzing findings, minimize the crash input before adding it to the
repository. Record the reproduction command, fuzzer, sanitizer profile, and
target harness in the pull request.

libsixel has several fuzz targets, including loader, CLI parser, and structured
API targets. If a fix affects normal CLI or library paths, consider adding a
normal regression test in addition to the fuzz reproducer.

## staticcheck

staticcheck is libsixel's pre-test lint/check suite. `make staticcheck` or
`meson compile -C builddir staticcheck` checks normal lint concerns plus
help/docs/completion synchronization, private includes, public API contracts,
test style, generated-file synchronization, and other project-specific rules.

If staticcheck fails, do not simply weaken the check. First decide which side
of the contract is correct. When staticcheck missed a real bug, consider
adding a small regression net in addition to fixing the bug itself.

## Portability

libsixel is used on Linux, macOS, Windows, BSD, Solaris/illumos-like systems,
Haiku, Emscripten, and more. When making changes, consider:

- whether the code builds without optional dependencies
- whether thread-disabled builds introduce unused variables or unresolved
  symbols
- whether paths, DLL handling, and environment handoff work on MSVC/clang-cl,
  MinGW, MSYS2, and Cygwin
- whether macOS-only APIs leak into other platform builds
- whether assumptions hold on pcc, tcc, musl, and Cosmopolitan libc
- whether assumptions hold under sanitizers, analyzers, Meson unity builds,
  and amalgamation builds
- whether Emscripten or cross builds accidentally execute host-only tools

When adding a platform-specific workaround, leave the reason in a comment or
commit message.

## Pull Request Checklist

- The pull request description explains the purpose of the change.
- Reproduction steps, verification commands, and unverified areas are listed.
- GitHub Actions CI has been checked.
- Bug fixes include a regression test, or explain why one was not added.
- CLI/API/docs/help/completion/bindings synchronization was checked.
- Autotools and Meson were both updated where needed.
- Build options, source lists, test registration, and install paths are aligned
  between the two build systems.
- Changed shell TAP tests pass shellcheck.
- Security issues were not reported publicly.
- Build artifacts and local experiment files are not included.

When in doubt, ask early in an issue or pull request. Small reproducible facts,
exact commands, and real logs make review and fixes much faster.
