# OpenVMS/GNV Autotools port notes

This note records the current OpenVMS/GNV Autotools status, the traps found
while bringing up `./configure` and `make`, and the intended next steps.  It
is separate from `openvms/build.com`, which is the native DCL bootstrap path.

## Goal

The GNV path should let the OpenVMS CI guest behave like the other ephemeral
SSH runners:

- run `bash ./configure` from a GNV shell
- build `src/.libs/libsixel.a` with the normal Autotools object list
- build a native OpenVMS shareable image with DCL `LINK`
- keep `img2sixel.exe` statically linked at first
- run a small `img2sixel` smoke test and verify SIXEL output bytes

The first milestone is intentionally narrow.  The GNV build does not need to
enable optional image backends, bindings, fuzz targets, or the test suite
before the static CLI and shareable-image paths are reliable.

## Current status

As of this note, `bash ./configure` completes on the OpenVMS/GNV guest when it
is run from a safe POSIX-style directory such as `/tmp/libsixel-openvms-build`.
The build can produce the static archive `src/.libs/libsixel.a`.

Manual native linking also works:

- DCL `LINK` can produce `converters/img2sixel.exe` from the converter objects
  and `src/.libs/libsixel.a`.
- The resulting `img2sixel.exe --help` path runs.
- A 2x2 PPM input can be converted to a SIXEL file.
- The smoke output starts with the expected DCS/SIXEL bytes and ends with ST.

The remaining blocker for normal `make` is the program link step.  GNV `gcc`
does not reliably create executables when it is invoked only with existing
object files and archives.

## GNV and Autotools traps

### Build directory

Do not run `configure` from a path whose absolute name contains a dollar sign,
for example `/SYS$SYSROOT/...`.  Autoconf and Automake shell machinery treat
that as an unsafe build directory later, and the resulting failure is hard to
understand.  The local `configure.ac` now detects this early and asks the user
to build under a path such as `/usr/tmp/libsixel-build`.

### Host detection

`config.sub` accepts `x86_64-dec-vms`, but `config.guess` can misdetect the
current x86_64 OpenVMS/GNV environment.  The configure script now detects
OpenVMS with `uname` before `AC_CANONICAL_HOST` and seeds the native build and
host aliases.

The same early block also adds `.exe` to `ac_executable_extensions`.  This
matters because early Autoconf and Automake program searches can run before
the compiler probe has learned the executable suffix.

### AWK and GNV executables

`AC_PROG_AWK` can find the GNV AWK executable once `.exe` suffix probing is
available early enough.  Avoid hard-coding `/bin/awk`; use the configured
`$(AWK)` value.

Generated helper scripts should avoid GNU-only command-line options unless the
GNV utility has been verified.  One example found during bring-up was `od`;
the completion embed generator now uses a plainer `od -tx1 -v` form instead
of relying on `od -An`.

### Parallel configure probes

The project has custom parallel header/function probes for normal Unix-like
systems.  On OpenVMS/GNV, background shell jobs can lose or misreport child
status, so those probes are serialized when `LIBSIXEL_OPENVMS` is active.

`config.status --jobs` is disabled on OpenVMS/GNV for the same reason.

### `config.status`

GNV `/bin/sh` has trouble with a few generated `config.status` patterns:

- EXIT traps can run while helper subprocesses are still using temporary AWK
  files.
- Some compound redirections around generated AWK fragments can fail even
  though the surrounding shell command does not make the root cause obvious.

The current configure logic post-processes `config.status` on OpenVMS/GNV:

- return to the original build directory before running `config.status`
- disable the generated EXIT trap
- split fragile compound redirections into simpler commands
- remove the temporary directory explicitly on normal exit

This is not beautiful, but it keeps the Autotools path inside the normal
`./configure` entrypoint instead of adding an outer configure wrapper.

### Header, type, and function probes

Several probes needed OpenVMS-specific handling:

- `ssize_t` needs an explicit fallback when the headers do not expose it.
- `struct timeval` must be checked as a complete type before code uses
  `gettimeofday`.
- Some function probes need real calls with relevant headers, not only the
  generic Autoconf "declare a function and link" pattern.
- GCC warning-flag probes should avoid flags that GNV GCC does not understand
  cleanly.

The rule of thumb is to keep `config.site` as a last resort.  Prefer probes
that describe the actual compiler and C RTL behavior.

### Assertions

The OpenVMS/GNV path currently defines `NDEBUG` during the build.  This avoids
an unresolved `assert` link failure seen while building the library objects.
This should be revisited after the native link wrapper is in place, because
the final diagnosis may belong to the C RTL setup or link option set rather
than to the source code itself.

### `getopt` and `getopt_long`

GNV can expose `getopt()` without a GNU-compatible `getopt_long()` header.
The converter code already has `converters/getopt_stub.h`; the OpenVMS path
now reuses the system `getopt()` globals while keeping the local long-option
shim.

### GNV `gcc` object-only links

This is the key remaining build-system issue.  A minimal reproduction is:

```sh
gcc -c hello.c -o hello.o
gcc -o hello hello.o
```

On the current GNV kit this exits with status 0 but does not create `hello`.
With `gcc -v`, the underlying DCL command reports a missing-parameter warning.
Native DCL `LINK` with the same object file does create an executable.

This is not primarily a libtool bug.  Libtool exposes the problem because it
normally links programs by invoking the compiler driver with object and
archive inputs.  The GNV release notes list this as a known limitation:
`GCC does not link executable files from object files`.

The documented workaround is to add an empty source file to the link command.
We do not want to rely on that workaround for libsixel, because it leaves the
build coupled to a GNV compiler-driver limitation.  The planned direction is
to bypass that limitation and invoke native OpenVMS `LINK` for final link
steps.

Reference:
<https://docs.vmssoftware.com/gnv-v3-0-2f-for-vsi-openvms-x86-64-release-notes/>

## Link-wrapper plan

The next GNV milestone is an OpenVMS native link wrapper.  It should accept
the compiler-style arguments emitted by Automake/libtool, translate the final
link step into a DCL option file, and call `/bin/dcl.exe` with native `LINK`.

The first version should support the exact paths needed by the current CI
guest and the current non-optional libsixel build:

- parse `-o <output>`
- collect `.o`, `.obj`, `.a`, and `.olb` inputs
- write a temporary DCL option file
- pass object files as ordinary linker inputs
- pass archives as `/LIBRARY`
- call `LINK /EXECUTABLE=<output> <option-file>/OPTIONS`
- verify that the output image exists before returning success

After program linking is stable, extend the same wrapper family for the
shareable image:

- generate a symbol-vector option file deliberately
- start with the public symbols needed by the smoke program
- expand toward the full public ABI in a controlled pass
- keep `img2sixel` static until the shareable ABI is ready

The wrapper should be conservative about POSIX-to-VMS path conversion.  The
first CI-oriented implementation can support the known guest layout, but the
code should keep conversion in one helper so that a more general mapper can be
added later.

## Integration plan

The preferred order is:

1. Add a small OpenVMS link wrapper under `openvms/` or `build-aux/`.
2. Teach `configure.ac` to select it only when `LIBSIXEL_OPENVMS` is active.
3. Use it for program link commands before trying to replace all libtool
   shared-library behavior.
4. Build `src/.libs/libsixel.a`.
5. Link `img2sixel.exe` statically through native `LINK`.
6. Add a GNV smoke target that converts a tiny generated PPM to SIXEL.
7. Add native shareable-image generation from the same object/archive set.
8. Only after those are stable, evaluate optional loaders and larger tests.

This keeps the port moving without hiding real OpenVMS differences behind a
large `config.site` or a dummy-source compiler workaround.

## Open questions

- Which path-conversion helper should be the long-term source of truth?
- Should the wrapper be used through `CCLD`, `LD`, or a libtool tag override?
- Which public API symbols should be exported first from the shareable image?
- How much of `make check` is worth enabling before optional image libraries
  are available?
- Can `assert` be restored after the native link path is deterministic?
