# OpenVMS Compatibility

## Scope

This document defines the maintained OpenVMS compatibility boundary and records the engineering rationale behind it. It is not a complete installation or CI-operations manual. Current support status belongs in [Build, Runtime, and Platform Support](../../platform-support.md), while the local runner architecture belongs in [CI Architecture and Design](../../ci/design.md).

The maintained configuration is OpenVMS 9.2-3 on x86-64 with GNV and Autotools. The source tree also retains a smaller native DCL bootstrap under [`openvms/`](../../../openvms/README.md). Keeping both paths was useful during bring-up: the DCL build proved that VSI C and the native linker could build the core before GNV, Autoconf, Automake, and libtool were made reliable.

## Maintenance contract

OpenVMS support is a compatibility path over GNV, not an assumption that GNV behaves like a conventional Unix host. Future changes must preserve the normal Autotools entry points while keeping demonstrated OpenVMS differences explicit and narrow. The source-level invariants are checked by [`tests/_static/sh/staticcheck-openvms-compat.sh`](../../../tests/_static/sh/staticcheck-openvms-compat.sh); an actual OpenVMS CI run remains required because a host-independent static check cannot exercise RMS, DCL, VSI C, the native linker, or GNV process semantics.

The following boundaries are part of the maintained contract:

- OpenVMS detection and `.exe` program lookup must occur before generic canonical-host and compiler discovery. Moving these decisions later can make configure fail before the platform branch is active.
- Header probes, function probes, `config.status`, recursive builds, and the default test runner must retain their documented serial boundaries unless a current GNV release is shown to preserve child statuses and generated-file state under parallel execution.
- Compiler, archiver, removal, and final-link behavior must remain isolated in the four wrappers under [`openvms/`](../../../openvms/README.md). Do not normalize a non-zero status merely because it is inconvenient: accept warning-only outcomes only when diagnostics are known and any requested output exists, and preserve all error or fatal evidence.
- Final program links must continue to use native DCL `LINK` for the affected Automake targets until object-only GNV compiler links are demonstrated to create and validate the requested executable. A successful shell status without an output image is a failure.
- OpenVMS process exits must remain shell-observable: zero represents success, failures use even non-zero condition values, and the inhibit-message bit prevents native condition text from corrupting TAP output. The unified C test runner performs this translation once at its process boundary so individual C tests can keep returning ordinary portable status values.
- Record-oriented file behavior, RMS timestamps, shell text conversion, PTY execution, and background-process semantics must not be generalized from POSIX behavior. Tests should assert the behavior they consume and use a narrow, documented skip only when the observation itself is unavailable on OpenVMS.
- Autotools source and intentionally tracked generated files must stay synchronized. Any change to an OpenVMS conditional in a `Makefile.am`, `configure.ac`, or `build-aux/*.in` file must be reviewed against the generated inputs used by a normal checkout.

### Change checklist

When changing configure logic, build orchestration, converter exit handling, test execution, or any script under `openvms/`:

1. Read the nearby OpenVMS comments and the rationale in this document before simplifying or consolidating the code.
2. Reproduce a suspected GNV tool failure with the smallest possible source or shell command and record both the process status and requested output state.
3. Update [`tests/_static/sh/staticcheck-openvms-compat.sh`](../../../tests/_static/sh/staticcheck-openvms-compat.sh) when the maintained invariant changes; do not weaken the check solely to accommodate an unrelated refactor.
4. Run the OpenVMS static check and the complete repository static suite on a conventional host.
5. Run the replacement revision through the maintained OpenVMS CI job and inspect the final job result. A queued job, a successful build phase without tests, or a warning-normalized command without its output is not green evidence.

### Known limitations

The continuously maintained path is OpenVMS 9.2-3 on x86-64 with the CI guest's GNV and VSI C environment. Alpha, Itanium, VAX, other OpenVMS releases, Meson, shared-library completeness, and interactive terminal behavior are not continuously verified. The Autotools path is intentionally serial in several phases, final executables use the native-link wrapper, and OpenVMS builds currently define `NDEBUG` because `assert()` can become unresolved on that link path. Revisit these limitations only with current toolchain evidence and an end-to-end OpenVMS run.

## Historical result

The port started with commit `5128e5a40` on May 26, 2026, reached a green GNV test run at `525c7b6dc` on May 28, and was merged from `port/openvms` into `develop` by `09521b70a` on June 2. Subsequent fixes removed assumptions about binary-file byte counts and clock-origin timing, and commit `229cf26d9` corrected a subtle M4/AWK quoting problem on August 7.

The resulting GNV path can configure the normal Autotools tree, build the library and command-line programs, and run the TAP suite. It deliberately does not pretend that GNV is an ordinary Unix host. OpenVMS-specific behavior is detected early and isolated in configure logic, narrow command wrappers, explicit link rules, phased build orchestration, C RTL compatibility code, and the test harness.

The central design rule was to preserve the standard `./configure` and Makefile entry points while bypassing only the GNV or OpenVMS behavior that had been demonstrated to fail. A large pre-populated `config.site`, a parallel build by default, and GNV's dummy-source link workaround were all rejected because they would hide rather than model the actual platform boundary.

## Bring-up sequence

1. The native [`openvms/build.com`](../../../openvms/build.com) path compiled the core into `libsixel.olb`, created a limited `libsixelshr.exe`, built a smoke program, and then built a statically linked `img2sixel.exe`.
2. [`configure.ac`](../../../configure.ac) learned to detect OpenVMS before `config.guess`, seed the host alias, recognize `.exe` during early program lookup, serialize unsafe probes, and repair the generated `config.status` script.
3. The GNV build produced `src/.libs/libsixel.a`; native DCL `LINK` was then introduced for final program links through [`openvms/gnv-link-program.sh`](../../../openvms/gnv-link-program.sh).
4. The useful command set grew to `img2sixel.exe`, `sixel2png.exe`, `lsqa.exe`, the TAP timer and timeout helpers, and `test_runner.exe`.
5. The TAP list and driver were adapted to record-oriented files and OpenVMS condition values. Platform-specific test assumptions were either replaced with portable checks or skipped with an explicit reason.
6. Build reliability work isolated object compilation, forced serial and verbose recursive makes, sanitized generated Makefiles, and added explicit build phases.
7. The final link wrapper learned to expand libtool archives, locate amalgamated objects, pass object files directly when native archive selection was unreliable, and reject unresolved-symbol diagnostics even when `LINK` produced an image.

## GNV and OpenVMS boundaries

### Working directory, host detection, and program lookup

GNV presents POSIX-like paths, but an absolute path such as `/SYS$SYSROOT/...` contains a dollar sign that later Autoconf and Automake machinery interprets as unsafe. Configure now rejects such a directory immediately with a specific diagnostic. CI uses a short POSIX-style work directory instead; this also reduces RMS name conversion and generated-path pressure.

`config.sub` accepted `x86_64-dec-vms`, but `config.guess` did not reliably identify the guest used for the port. Configure therefore checks `uname` before `AC_CANONICAL_HOST`, derives an OpenVMS CPU name, and seeds the build and host aliases. It also adds `.exe` to `ac_executable_extensions` before `AC_PROG_AWK` and other early searches. This matters because many GNV programs are installed as names such as `awk.exe`, `make.exe`, and `realpath.exe`.

The private CI launcher complements source-side detection with `.exe` aliases for the tools needed before and during configure. Those aliases are exported through `BASH_ENV` so configure-generated child scripts see the same command environment. The source tree itself still uses discovered programs such as `$(AWK)` rather than hard-coded `/bin` paths.

### Configure probes and `config.status`

Background jobs in the GNV shell could lose or misreport child status. The project's parallel header and function probes are therefore forced to one worker on OpenVMS, and `config.status --jobs` is disabled there. Serial execution is a correctness boundary, not merely a conservative performance setting.

The generated `config.status` exposed additional shell failures: helper subprocesses could run the EXIT trap while temporary AWK files were still in use, and compound redirections around generated fragments could fail obscurely. The OpenVMS branch in [`AC_OUTPUT_PARALLEL`](../../../configure.ac) returns to the original build directory, disables the generated trap, splits the fragile redirections, preserves the real exit status, and performs normal-exit cleanup explicitly.

Function detection also needed to model the C RTL rather than rely on Autoconf's prototype-free fallback. OpenVMS can expose a POSIX name through headers or macros even when a plain lowercase linker-symbol probe fails. Configure rechecks selected functions with their real headers and a real call. This is why the port prefers targeted probes and small compatibility declarations over a broad cache file that simply claims functions exist.

The compiler discriminator `__VMS` selects `_XOPEN_SOURCE_EXTENDED` before system headers in the standalone threading source and the generated amalgamation prologue. The C RTL hides `struct timespec` under strict feature namespaces without this extension, even though pthread timed waits require it. The deadline uses the existing `sixel_compat_gettimeofday()` wall-clock wrapper so the pthread implementation does not require an unconditional `clock_gettime()` call. The [amalgamation description](../../build/autotools.md) explains why the generated prologue must also select the namespace.

### GNV Make 3.78.1 and generated Makefiles

The default Unix-style GNV make is version 3.78.1. During the port it lost or misread state in several patterns generated by modern Automake: silent recursive makes could poison the following libtool target, a target with many libtool-object prerequisites could report failure after successful compilation, long generated comments and tag prerequisites could hang parsing, and multiline object assignments could disappear before library or test-runner links.

The durable workarounds are split across [`Makefile.am`](../../../Makefile.am), [`src/Makefile.am`](../../../src/Makefile.am), [`tests/Makefile.am`](../../../tests/Makefile.am), and the `openvms-makefiles` command in [`configure.ac`](../../../configure.ac):

- OpenVMS recursive builds use `-j1 V=1`; test sharding remains a separate choice.
- Library objects are built through one recursive make per object so each compile begins with fresh make state.
- CI invokes explicit include, amalgamation preparation, library, phase-two, and phase-three targets instead of trusting a full recursive traversal to return the correct status.
- Generated Makefiles have inert tag metadata and problematic long comment records removed on OpenVMS.
- The final library and test-runner object lists are restated on one physical line, followed by explicit target prerequisites.
- Dependency tracking defaults to disabled because its Makefile bootstrap can hang after `config.status` has already produced the build files.

The Makefile sanitizer itself contained a delayed-expansion trap. A bare AWK `$0` inside an M4 macro argument was read as the enclosing M4 macro name and repeatedly rescanned when Autoconf generated `config.status`; autoreconf time grew from seconds to more than twenty minutes. The source now spells the AWK record variable as `$[0]`, which M4 reduces to the intended `$0` at the correct expansion stage.

The [amalgamation generator](../../../tools/gen-amalgamation.sh) keeps source paths and their preprocessor guards in parallel temporary files. It computes all guards in one AWK pass, avoiding per-test command substitutions and `sed`/`tr` pipelines. This also keeps the growing source inventory out of large GNV shell variables. Changes to guard generation must preserve the generated translation unit byte for byte for the same source and configuration inputs.

### Final program linking

The native-link wrapper converts paths in the current shell and reuses the resolved directory for consecutive inputs from that directory. Directory-component escaping also uses shell parameter expansion. Per-object nested command substitutions and `sed` pipelines can exhaust GNV bash memory when the unified test runner links thousands of objects, before native `LINK` is even invoked.

The wrapper runs `LINK` through a short DCL procedure that redirects native `SYS$OUTPUT` and `SYS$ERROR` to separate logs. GNV shell redirection alone can leave an empty log while native diagnostics go to the terminal, defeating unresolved-symbol detection. Both native logs are normalized and inspected before the wrapper accepts an image. Test headers containing function bodies must also be guarded by their owning `BUILD_TEST_*` selections in the amalgamation; an unoptimized library build must not acquire references to test-only helpers.

The decisive GNV limitation was reproducible outside libsixel:

```sh
gcc -c hello.c -o hello.o
gcc -o hello hello.o
```

On the porting guest the second command could exit successfully without creating an executable. The [GNV 3.0-2F release notes](https://docs.vmssoftware.com/gnv-v3-0-2f-for-vsi-openvms-x86-64-release-notes/#known-issues-and-limitations) document the same object-only link limitation and suggest adding an empty source file. libsixel did not adopt that workaround.

Instead, [`openvms/gnv-link-program.sh`](../../../openvms/gnv-link-program.sh) accepts the compiler-style arguments generated by Automake, converts POSIX paths to OpenVMS syntax, emits a DCL option file, and invokes native `LINK`. It is selected only for affected program targets; compilation and the rest of Autotools remain on the normal path.

The wrapper also handles two native-linker details that appeared later in the port:

- Passing a libtool archive only as `/LIBRARY` did not reliably select all objects, especially when an amalgamated build placed a large translation unit in one archive member. The wrapper first expands archive members or finds libtool object files and lists those objects directly, then falls back to the archive.
- An OpenVMS warning condition is not enough to decide success. The wrapper normalizes the native log, rejects undefined-symbol diagnostics, and verifies that the requested image exists before returning success.

This link path is intentionally conservative. It ignores only known compiler and linker flags, accepts only supported library options, and fails on unknown link inputs rather than silently dropping them.

### Condition values and cleanup status

OpenVMS condition values do not map directly onto the POSIX convention that zero is success and every non-zero value is failure. Pure warnings from VSI C or the native `LIBRARY` utility can surface through GNV as non-zero shell status even after producing the requested object or archive. Conversely, a low-bit condition value such as plain `1` can look successful to GNV bash.

Three narrow wrappers preserve the evidence needed to translate that boundary:

- [`openvms/gnv-cc.sh`](../../../openvms/gnv-cc.sh) prints the compiler log unchanged and maps only warning-only outcomes to success. Errors, fatal conditions, GCC driver diagnostics, and a missing requested object remain failures.
- [`openvms/gnv-ar.sh`](../../../openvms/gnv-ar.sh) accepts native `LIBRARY` warning conditions such as `%LIBRAR-W-COMCOD` while preserving error and fatal statuses.
- [`openvms/gnv-rm.sh`](../../../openvms/gnv-rm.sh) avoids invoking `rm.exe` for absent forced-removal operands and harmless directory operands that generated cleanup rules expect `rm -f` to ignore.

Converter process exits use zero for success and even non-zero OpenVMS condition values for failure. They also set the inhibit-message bit so DCL does not inject a condition message into TAP output. The TAP driver exports the OpenVMS-specific mapped-error range so tests assert the shell-visible contract rather than Unix numeric folklore.

The unified C test runner preserves POSIX exit values through the C RTL's `__posix_exit` entry point. In particular, status `77` must reach GNV bash unchanged so the PTY and SIMD tests can report an unavailable capability as a TAP skip. Collapsing every nonzero result into one native failure condition loses that distinction. The explicit RTL call also works when an amalgamated translation unit has already included `stdlib.h`, too early for a local `_POSIX_EXIT` definition to remap `exit`. See the POSIX-style exit section of the [VSI C RTL reference](https://docs.vmssoftware.com/vsi-c-run-time-library-reference-manual-for-openvms-systems/).

### C RTL declarations and semantics

The GNV headers and VSI C RTL did not always expose the same combination of types, prototypes, macros, and linkable entry points. The port added focused handling for `ssize_t`, incomplete or absent `struct timeval`, `gettimeofday`, `fsync`, `usleep`, `strnlen`, `strerror_r`, `setenv`, and related interfaces. These shims are guarded by configure results and `LIBSIXEL_OPENVMS`; they are not unconditional substitutes for the platform implementation.

VSI C can expose `NAN` and `INFINITY` through runtime values instead of constant expressions. Portable C sources must not use those macros to initialize static-storage objects, including function-local `static` objects and file-scope objects without an explicit `static` qualifier. Use an automatic local object or runtime assignment when a nonfinite specimen is required. The optional [C AST check](../../../tests/_static/sh/staticcheck-c-ast.sh) inspects declarations and initializer expressions in the library, tools, integrations, examples, and tests. It distinguishes automatic storage, runtime assignment, comments, strings, and unevaluated `sizeof`/alignment operands. It checks direct macro identifiers in all preprocessor branches; it does not expand macro aliases or evaluate conditional compilation. The check includes in-memory positive and negative fixtures and requires the `tree_sitter` and `tree_sitter_c` Python packages; missing packages produce an explicit skip.

Two semantic cases deserved dedicated code. OpenVMS `putenv()` retains the supplied buffer, so the `setenv` compatibility implementation must not free a successful entry. The `strerror_r()` symbol could be detectable without a usable public declaration in some compilation units, so those units use `strerror()` plus a bounded copy instead of relying on an implicit prototype.

GNV exposed `getopt()` without a matching GNU `getopt_long()` interface, and its short-option parser stops at the first positional argument. The converters therefore use the project's fallback with private parser symbols and GNU-style option permutation. The fallback rejects attached values on flags that take no argument, including an empty `=`, so `lsqa --direct=yes` and `lsqa --direct=` remain invalid just as they are with the system long-option parser.

OpenVMS builds currently add `-DNDEBUG` because `assert()` can become an unresolved external reference on the GNV link path. This remains a visible limitation rather than a completed portability result; it should be revisited if the native linker integration or C RTL setup changes.

### Record-oriented files, time, and tests

RMS record semantics invalidate several assumptions that look harmless on byte-stream filesystems. A very large test list substituted as one Make variable record could be rejected, so OpenVMS configure writes `tests/check-tests.list` one pathname per line and the custom [`tests/Makefile.am`](../../../tests/Makefile.am) runner consumes it directly. Small binary fixtures can also be exposed through record formats that make `wc -c` unsuitable as a portability guard; tests now validate the behavior they need rather than byte-counting an otherwise unused seed.

The first image smoke test used a small ASCII PPM because creating a binary PPM from the GNV shell could append a text newline. The lesson is not that OpenVMS cannot process binary input, but that a shell text producer is not proof of binary-safe fixture creation.

The same rule applies to palette tests. A shell `printf`, `dd`, pipeline, or redirection can create an RMS record file whose logical contents differ from the byte stream intended by an ACT, RIFF PAL, CR-only, CRLF, or BOM test. Byte-sensitive palette inputs and expected outputs are therefore tracked fixtures under [`tests/data/inputs/mapfile/`](../../../tests/data/inputs/mapfile/) and are marked `-text` in [`.gitattributes`](../../../.gitattributes). [`tests/_static/data/openvms-record-fixtures.tsv`](../../../tests/_static/data/openvms-record-fixtures.tsv) locks their byte counts and POSIX `cksum` values. Tests that inspect fields in a generated binary file use `stat` for the OpenVMS physical size and hexadecimal `od` output for field bytes instead of materializing another binary file through the shell. A byte-exact stdout test pipes the converter stream directly into `cmp -` before RMS file creation can add a record terminator or make `cmp` consider record attributes that are not part of the payload.

When an image-quality test saves a SIXEL capture for later decoding, use the converter `-o` option to create the file. GNV shell redirection can create a text-record file: a packet exceeding the RMS record-size limit can produce `invalid record size` and lose earlier bytes. A converter-owned binary descriptor preserves the stream without changing image dimensions, quality thresholds, or the samples under test.

A test that only needs the converter exit status must not redirect an otherwise unobserved SIXEL stream into an artifact file. On GNV that can fail at the RMS record boundary before the behavior under test is reached. Pass `-o/dev/null` to the converter instead; retain an output file only when the test reads or compares it.

Feed a finite reference image from a regular file when a test observes decoded pixels rather than pipeline behavior. GNV can retain the writer side of a short `printf | img2sixel` or `printf | lsqa` pipeline and prevent the reader from observing EOF. The multi-pass late-error decoder regression reads a tracked capture containing a valid first image and an incomplete final DCS; this tests rejection before PNG publication without relying on pipe EOF. The truncated-ASCII PNM regression writes its complete literal red-and-black P3 reference to a file and reads it through the strict loader path. The Duotone value test reads the PSD and independent Lab-derived PPM directly from regular files. The direct reconstruction checks likewise measure a saved PNG reference while retaining the actual SIXEL stdin path. These tests preserve their independent image references and numerical assertions without depending on that pipe lifetime. The PNM test changes into its artifact directory before creating and comparing files, because GNV file utilities can resolve an absolute Unix prefix as a native logical device; relative filenames avoid that ambiguity.

The TAP harness supplies a `cmp` wrapper that shortens absolute build-tree operands before invoking the native utility. GNV's `exec` can return after a native image finishes, so the wrapper explicitly exits with that comparison's status. Relative operands must be compared exactly once in the caller's directory; falling through to the absolute-path branch changes directory and can turn an equal comparison into a missing-file error. Equal, different, and unreadable inputs retain `cmp` statuses `0`, `1`, and `2` respectively.

RMS also keeps an exclusive writer lock on a converter-owned output file. A test must close that output descriptor before reopening the capture for validation. The high-color macro tests switch the encoder sink to `/dev/null` after each completed frame, which closes the capture while preserving encoder state for output-reuse and animation-replay assertions. This observes completed file output; it does not require concurrent readers to share an active writer. The process-wide timeline writer likewise closes its handle at flush boundaries and reopens in append mode for subsequent events. This makes completed sessions readable before process exit without losing earlier records.

Output-failure tests use a child path beneath an existing regular fixture to force an open failure. Do not use `.` as an assumed invalid output filename: native OpenVMS path interpretation differs from POSIX directory syntax.

Shell support for `/dev/stderr` does not imply that the native C runtime can reopen that redirected stream by filename. Timeline assertions set `SIXEL_LOG_PATH` to a regular file owned by the converter and read it after process exit. The [OpenVMS staticcheck](../../../tests/_static/sh/staticcheck-openvms-compat.sh) and its [source scanner](../../../tests/_static/awk/native-stream-paths.awk) reject literal `/dev/stdin`, `/dev/stdout`, `/dev/stderr`, `/dev/fd/N`, and `/proc/self/fd/N` paths in native `fopen`, `freopen`, `open`, `open64`, and `sixel_compat_fopen` calls and in `SIXEL_LOG_PATH` settings. Coverage includes C sources and headers, shell scripts, TAP `*.t` files, Make inputs, and Meson inputs; comments and line continuations are handled. Regular filenames, `/dev/null`, and shell descriptor redirection remain valid. This is a literal-source guard, not data-flow analysis of filenames assembled through variables, macros, or helper functions. Its in-memory positive and negative cases verify the detection boundary without compiling or running native programs.

Clock behavior is another platform boundary. Forcing `TZ=UTC0` in CI shifted the time GNV make compared with RMS modification times, producing false future-timestamp and clock-skew warnings. The OpenVMS job leaves `TZ` unset. Tests that require a stable monotonic relationship between platform clock origins are skipped explicitly when that relationship was not demonstrated on GNV.

Signal, pipeline, and background-job semantics were handled test by test. The port did not blanket-skip the suite: it exported `RUNTIME_ENV_IS_OPENVMS`, replaced avoidable assumptions, and retained targeted skips only where the observation itself was not portable.

## CI execution and performance lessons

The local CI kept OpenVMS within the existing Unix-VM backend but isolated guest-specific command launch. A normal non-interactive OpenSSH exec channel could return before a GNV child finished or fail to stream progress. The runner therefore allocates a PTY, enters GNV from DCL, streams the terminal output, and combines the PTY result with a remote status file. This is a runner workaround, not behavior required of libsixel users.

File creation dominated more than CPU throughput. The CI job applies best-effort native tuning that disables highwater marking for the work volume, limits worktree file versions, and purges older versions. Failure of that tuning is reported but does not fail the build because it affects speed rather than correctness.

The maintained CI configure flags are:

```sh
./configure --disable-dependency-tracking --enable-amalgamated-lib
```

The amalgamated library was the largest measured speedup. A known-green pre-amalgamated run at `9812c7234` spent about 1,822 seconds in the build phase and 1,341 seconds in tests, with a 3,480-second job duration. A known-green run at `bfe6df35d` after the amalgamated path was corrected spent about 350 seconds in the build phase and 1,421 seconds in tests, with a 2,100-second job duration. These are historical CI observations rather than a current benchmark, but they explain why reducing object and archive-member churn mattered more than adding virtual CPUs.

The first complete green baseline at `525c7b6dc` reported 4,550 tests: 2,619 passed, 1,928 skipped, 3 were expected failures, and none failed or errored. Its recorded test elapsed time was about 1,677 seconds. A two-shard mode was implemented for experiments, but process startup and file I/O made it slower or less reliable on the runner. [`OPENVMS_TEST_JOBS`](../../../tests/Makefile.am) therefore defaults to `1`; the build recursion is independently fixed at `-j1 V=1`.

Because the explicit build phase has already completed `all`, CI runs the test subdirectory directly:

```sh
make -C tests V=1 check-am OPENVMS_TEST_JOBS=1
```

Using top-level `make check` would re-enter the expensive library-object walk before starting TAP. The split is an optimization in CI orchestration, not a claim that the top-level target has different test semantics.

## Reusable lessons

- Treat GNV as a compatibility layer over OpenVMS, not as proof that every Unix process, status, path, and file assumption holds.
- Reproduce a toolchain failure with a minimal program before assigning it to Autoconf, Automake, libtool, or project code.
- Normalize native warning statuses only when an expected output exists and the captured diagnostic contains no error or fatal condition.
- Prefer explicit build phases and observable progress markers when the shell or make implementation cannot reliably preserve child state.
- Reduce filesystem and archive churn before adding parallelism on a record-oriented, high-latency guest.
- Keep CI transport workarounds in the runner and source compatibility workarounds in the source tree; neither should silently absorb failures owned by the other.
- Preserve platform-specific skips as narrow statements about the missing observation, not as a general exemption from the test suite.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| OV-01 | Configure detects OpenVMS and `.exe` tools early, serializes unsafe probes and `config.status`, selects the narrow wrappers, disables dependency tracking by default, and preserves M4-safe AWK generation. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-02 | OpenVMS build and test defaults remain serial, library compilation retains its isolated-object path, and exactly the seven affected programs, including `progressive-study`, use the native-link override. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-03 | The compiler and archiver wrappers accept representative warning-only conditions but reject error conditions, while forced removal of an absent file remains harmless. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-04 | The native-link wrapper rejects unresolved-symbol diagnostics and unsupported arguments and verifies that the requested image exists. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-05 | Converter failures retain even OpenVMS condition severities with the inhibit-message bit, and the TAP driver accepts the documented mapped-error range. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-06 | `LIBSIXEL_OPENVMS` remains classified in the cross-platform macro ledger and this policy remains linked from the platform index. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| OV-07 | Byte-exact DCS assertions discard only GNV's trailing text-record newline, and converter-failure tests accept only the harness-defined mapped-error range. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-08 | Palette fallback tests use the bounded 64 x 64 fixture so redirected SIXEL output cannot exceed the demonstrated RMS record-size boundary before the fallback contract is observed. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-09 | Palette parser tests use byte-stable tracked fixtures for binary records, line endings, BOMs, and whitespace, while generated binary field checks use OpenVMS-safe size and hexadecimal-byte observations. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-10 | Tests that observe only converter success discard SIXEL through the converter output option instead of creating an unobserved RMS-sensitive output record. | [tests/processing/dither/general/0006_cropping_with_alternate_dither.t](../../../tests/processing/dither/general/0006_cropping_with_alternate_dither.t), [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-11 | Every process return from the unified C test runner preserves the POSIX result, including skip status 77, through the C RTL exit boundary. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh), [tests/cli/color/tty/0001_roundtrip.t](../../../tests/cli/color/tty/0001_roundtrip.t), [tests/processing/frame/0093_frame_float_mask_coverage_avx.t](../../../tests/processing/frame/0093_frame_float_mask_coverage_avx.t) |
| OV-12 | A byte-exact stdout assertion compares the live converter stream before an RMS file can add a record terminator, and the static guard prevents reintroducing the intermediate file. | [tests/quant/mapfile/0119_mapfile_export_act_stdout_exact.t](../../../tests/quant/mapfile/0119_mapfile_export_act_stdout_exact.t), [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-13 | Finite reference images, incomplete captures, and decoded-sample observations reach native converters and the C test runner through regular files, avoiding dependence on EOF from short GNV pipelines. | [tests/cli/sixel2png/0017_multi_pass_late_error.t](../../../tests/cli/sixel2png/0017_multi_pass_late_error.t), [tests/cli/options/regression/0096_loader_builtin_pnm_truncated_ascii_image_regression.t](../../../tests/cli/options/regression/0096_loader_builtin_pnm_truncated_ascii_image_regression.t), [tests/loader/builtin/1293_loader_builtin_psd_duotone_alternate_lab_samples.t](../../../tests/loader/builtin/1293_loader_builtin_psd_duotone_alternate_lab_samples.t) |
| OV-14 | The generated `cmp` wrapper compares relative or rewritten absolute operands exactly once in the intended directory, preserving statuses 0, 1, and 2 even when native-image `exec` returns to GNV. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-15 | C AST inspection rejects direct `NAN` and `INFINITY` operands in static-storage initializers while permitting automatic initializers and runtime assignments. | [tests/_static/sh/staticcheck-c-ast.sh](../../../tests/_static/sh/staticcheck-c-ast.sh) |
| OV-16 | Completed fd-backed macro captures are closed before RMS readers reopen them, while the encoder remains alive for subsequent phases. | [tests/codec/macro/0017_highcolor_p3_define.t](../../../tests/codec/macro/0017_highcolor_p3_define.t) |
| OV-17 | Quality-test captures use converter-owned output files instead of GNV shell text records, retaining full SIXEL packets without reducing the image or quality threshold. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |
| OV-18 | Flushing the shared timeline releases RMS writer locks so completed sessions are readable in the same process, and later sessions append without truncating earlier records. | [tests/processing/timeline/0006_timeline_encode_frame_session.t](../../../tests/processing/timeline/0006_timeline_encode_frame_session.t) |
| OV-19 | An output-open failure beneath an existing regular input preserves the previously configured PNG destination and configuration revision. | [tests/processing/progressive/0013_failed_output_preserves_destination.t](../../../tests/processing/progressive/0013_failed_output_preserves_destination.t) |
| OV-20 | Literal Unix stream aliases do not reach native file-opening APIs or timeline destinations in maintained C, shell, TAP, Make, and Meson sources. | [tests/_static/sh/staticcheck-openvms-compat.sh](../../../tests/_static/sh/staticcheck-openvms-compat.sh) |

### Coverage boundary

The static check runs on non-OpenVMS hosts and protects source structure plus representative wrapper translations. It cannot validate VSI compiler diagnostics, DCL `LINK`, RMS record and timestamp behavior, GNV child-status propagation, PTY transport, or end-to-end test execution. Those remain covered only by the maintained OpenVMS CI job, whose current result must be checked after a compatibility-sensitive change.

## Source and history map

The most useful starting points for future maintenance are:

- [`configure.ac`](../../../configure.ac): early platform detection, serialized probes, `config.status` repair, wrapper selection, C RTL probes, test-list generation, and generated-Makefile sanitation.
- [`Makefile.am`](../../../Makefile.am), [`src/Makefile.am`](../../../src/Makefile.am), and [`tests/Makefile.am`](../../../tests/Makefile.am): serial verbose recursion, phased targets, isolated object builds, and the OpenVMS TAP runner.
- [`openvms/gnv-link-program.sh`](../../../openvms/gnv-link-program.sh), [`openvms/gnv-cc.sh`](../../../openvms/gnv-cc.sh), [`openvms/gnv-ar.sh`](../../../openvms/gnv-ar.sh), and [`openvms/gnv-rm.sh`](../../../openvms/gnv-rm.sh): the four narrow command-boundary adapters.
- [`openvms/README.md`](../../../openvms/README.md) and [`openvms/build.com`](../../../openvms/build.com): the native DCL bootstrap and its deliberately limited shareable-image scope.
- Commits `5128e5a40` through `09521b70a`: the original port series. Within it, `76586712f` starts GNV host detection, `12b1fa883` adds native program linking, `525c7b6dc` reaches the first green suite, `044147add` through `6447d69df` normalize command statuses, `b1a314f5a` repairs generated Makefiles, and `5d82f8302` through `e8fb0ff67` harden libtool-archive linking.
- Commits `f76443caa`, `fc8ab0404`, and `229cf26d9`: later corrections for record-oriented binary checks, unstable clock-origin assumptions, and M4-safe AWK generation.

When revisiting the port, inspect the current code and CI definition in addition to these commits. The commit series explains why the boundaries exist; the maintained sources define what is true now.
