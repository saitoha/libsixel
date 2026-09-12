# Autoconf Macros and Configuration Extensions

The Autotools build has two macro sources: imported definitions under `m4/`, and project-specific `AC_DEFUN` definitions inside [configure.ac](../../configure.ac). This distinction matters when looking for the project's parallelization work: the CPU-count helper is in `m4/`, but the parallel header, function, and output drivers are defined in `configure.ac` itself.

These macros generate shell code. They do not run as an M4 interpreter during an ordinary C build. A maintainer expands them into the tracked `configure` script; a source recipient runs that script and its support programs. [Why generated files are committed](autotools.md#why-generated-files-are-committed) explains the distribution policy, and [support scripts](support-scripts.md) describes the execution side.

## From macro source to a configured build

```text
configure.ac + m4/*.m4 + installed Autoconf/Automake macros
    | aclocal / autoconf / autoheader, during maintainer regeneration
    v
aclocal.m4 + configure + config.h.in
    | ./configure, on the machine configuring the build
    v
compiler and library probes + config.status
    | substitute Makefile.in, header templates, script templates
    v
Makefiles + config.h + generated libtool + configured test helpers
```

`AC_CONFIG_MACRO_DIR([m4])` identifies the local macro directory. `aclocal` assembles or includes the needed definitions in `aclocal.m4`; Autoconf expands them into `configure`. `AC_CONFIG_AUX_DIR([build-aux])` separately identifies executable support files such as `config.guess`, `config.sub`, `ar-lib`, and `ltmain.sh`. A macro directory and an auxiliary-script directory solve different parts of the build-generation problem.

Three common outputs should be distinguished. `AC_DEFINE` supplies C preprocessor definitions, normally through `config.h`; `AC_SUBST` supplies text values for configured templates such as `Makefile.in`; `AM_CONDITIONAL` selects Automake-generated rule branches. A successful library probe may need more than one of these outputs, plus a corresponding Meson implementation.

## The tracked `m4/` inventory

| File | Provider and purpose | Use in libsixel |
| --- | --- | --- |
| [ax_count_cpus.m4](../../m4/ax_count_cpus.m4) | Autoconf Archive `AX_COUNT_CPUS`. | Produces `CPU_COUNT` for configure probe concurrency and default recursive-make job flags. |
| [ax_gcc_builtin.m4](../../m4/ax_gcc_builtin.m4) | Autoconf Archive `AX_GCC_BUILTIN`. | Probes `__builtin_unreachable` in the non-MSVC configuration branch. |
| [ax_gcc_func_attribute.m4](../../m4/ax_gcc_func_attribute.m4) | Autoconf Archive `AX_GCC_FUNC_ATTRIBUTE`. | Probes the `deprecated` function attribute outside the excluded MSVC/OpenVMS branches. |
| [ax_gcc_var_attribute.m4](../../m4/ax_gcc_var_attribute.m4) | Autoconf Archive `AX_GCC_VAR_ATTRIBUTE`. | Probes the `deprecated` variable attribute alongside the function-attribute check. |
| [ax_ruby_ext.m4](../../m4/ax_ruby_ext.m4) | Autoconf Archive `AX_RUBY_EXT`. | Queries the selected Ruby interpreter's extension metadata when Ruby is enabled and found. |
| [pkg.m4](../../m4/pkg.m4) | pkg-config macro collection. | Finds `pkg-config`, checks optional dependency modules, obtains compile/link flags and package-defined paths. |
| [libtool.m4](../../m4/libtool.m4) | Libtool's main configure macro collection. | `LT_INIT` probes compiler/linker/library conventions and supplies the platform configuration used by generated `libtool`. |
| [ltoptions.m4](../../m4/ltoptions.m4) | Libtool option definitions. | Handles Libtool's macro options and shared/static/PIC-related policy. |
| [ltsugar.m4](../../m4/ltsugar.m4) | Libtool M4 utility layer. | List, string, and dictionary operations used while expanding its macros. |
| [ltversion.m4](../../m4/ltversion.m4) | Libtool package/revision identity. | Keeps the macro expansion associated with its Libtool version; the tracked copy declares 2.5.4. |
| [lt~obsolete.m4](../../m4/lt~obsolete.m4) | Libtool compatibility definitions for `aclocal`. | Prevents obsolete macro names from pulling in an incompatible older system Libtool macro set. |

The `AX_` prefix and embedded upstream headers identify imported macros, not a family invented by this project. Conversely, an `AC_` prefix on a definition inside `configure.ac` does not make it an upstream Autoconf feature. Follow the definition, not just the naming convention.

## CPU count and project scheduling

`AX_COUNT_CPUS` tries available operating-system interfaces and utilities, including `getconf`, `nproc`, platform-specific `sysctl` or processor queries, and Windows processor metadata. It counts available logical processors where the selected interface supports that distinction and defaults to one if detection fails. The macro does not itself launch configure probes or change make's jobserver.

The project uses the resulting `CPU_COUNT` in two places. The parallel probe macros bound batches of compiler work; the configure logic also prepares `LS_DEFAULT_MAKEFLAGS` for recursive builds when job flags were not supplied. The root Makefile still respects explicit `-j` and inherited jobserver state. These are build-time scheduling choices, separate from libsixel's runtime thread budget.

OpenVMS/GNV selects conservative single-worker probe and output paths because child-status and shell semantics can invalidate otherwise reasonable parallel execution. The [Autotools orchestration chapter](autotools.md#limitations-and-local-remedies) and [OpenVMS ledger](../misc/platforms/openvms.md) explain these boundaries.

## Compiler capability macros

`AX_GCC_BUILTIN` emits a small link test using a known argument pattern for the requested builtin, caches the result in an `ax_cv_have_*` variable, and defines a corresponding `HAVE_*` macro on success. The `GCC` name describes the builtin interface; other compilers can implement the same interface. The checked-in macro includes many possible probes, but libsixel's call site currently requests `__builtin_unreachable`. The inventory of cases inside the macro is not the inventory of probes this project runs.

The function- and variable-attribute macros likewise link small specimens and inspect diagnostics. An unsupported GNU-style attribute can be ignored with only a warning, so accepting a successful compiler exit alone would produce false positives. These macros reject nonempty `conftest.err` on their successful-link branch. Suppressing relevant warnings can therefore change the meaning of the probe. Their results include `HAVE_FUNC_ATTRIBUTE_DEPRECATED` and `HAVE_VAR_ATTRIBUTE_DEPRECATED`; `configure.ac` also owns the separate annotation substitutions used by [sixel.h.in](../../include/sixel.h.in).

These are compile/link capability checks, not execution tests or compiler-version comparisons. Keep the actual probe specimen, flags, cached answer, and downstream use together when changing them. In particular, a compiler adapter such as [pcc-meson](support-scripts.md#pcc-translate-compiler-identity-and-selected-preprocessing) can make the Meson detection path use different tools from the Autoconf path.

## Ruby metadata and dependency discovery

`AX_RUBY_EXT` queries `RbConfig` for Ruby's version, include and extension installation directories, preprocessor flags, linker flags, and extension suffix. It accepts overrides through `RUBY_EXT_*` variables. Its Darwin branch removes architecture flags from the retrieved linker flags and adds bundle/dynamic-lookup flags. `configure.ac` invokes it conditionally after finding Ruby, then separately checks the Fiddle library used by the binding. The macro supplies configuration metadata; it does not build or install the test gem. That later operation belongs to [the binding environment resolvers](test-harness.md#shared-binding-package-environments).

`pkg.m4` translates package metadata into configure results. The call sites use `PKG_PROG_PKG_CONFIG`, `PKG_CHECK_MODULES`, `PKG_CHECK_VAR`, and `PKG_CHECK_EXISTS` for optional libraries and integration paths. For example, GdkPixbuf requires both dependency flags and metadata describing its module directory and loader-query tool. The prefix supplied to `PKG_CHECK_MODULES` determines variables such as `LIBJPEG_CFLAGS` and `LIBJPEG_LIBS`.

The macro collection offers more entry points than the project uses. Feature selection, fallback searches, static consumer metadata, and whether a missing dependency is fatal remain the responsibility of `configure.ac`. Meson's `dependency()` calls and project probes independently implement its side of the feature contract; they do not execute `pkg.m4`.

## Libtool's macro and shell halves

`libtool.m4` probes the target's library conventions: compiler/PIC flags, symbol extraction, archive and shared-library commands, suffixes, runtime search variables, and installed versus build-tree linking. The companion `lt*.m4` files support that expansion and its compatibility requirements. [ltmain.sh](../../build-aux/ltmain.sh) supplies the shell command machinery used with those results in the generated `libtool` program.

The less obvious `lt~obsolete.m4` has a practical purpose. `aclocal` recognizes historical `AC_DEFUN` names differently from newer internal M4 definitions. Without the compatibility definitions it can discover an old name in the system macro directory and import the wrong Libtool file. The deliberately late-sorting filename and placeholder definitions keep the local macro set coherent.

The tracked Libtool macros and `ltmain.sh` must be treated as a related set when regenerating or upgrading. Historical local patches are not proof of the current contents: tool regeneration can replace imported files. Project-level MSVC selection and import-library rules also live in `configure.ac` and [src/Makefile.am](../../src/Makefile.am), so reading only `m4/libtool.m4` misses part of the adaptation. [MSVC adapters](support-scripts.md#msvc-compiler-archiver-and-library-linking-are-separate) separates the compiler, librarian, and Libtool responsibilities.

## Project macros defined in `configure.ac`

| Macro | Work generated | Important boundary |
| --- | --- | --- |
| `AC_CHECK_GCC_DIAG_WBUILTIN_DECL_MISMATCH` | Checks whether the compiler accepts the diagnostic pragma used around function probes. | Its cached answer controls whether the parallel function probe emits that pragma. |
| `AC_CHECK_HEADERS_PARALLEL` | Batches independent header preprocessing probes in per-header directories, then imports results. | Checks preprocessing availability with the supplied includes; it does not prove every use of a header will compile. |
| `AC_CHECK_FUNCS_PARALLEL` | Batches independent function link probes in per-function directories, then imports results. | Checks symbol linkability with the selected flags and libraries, including stub rejection; does not run the functions. |
| `AC_OUTPUT_PARALLEL` | Generates `config.status`, checks for `--jobs` support, and invokes it explicitly. | Preserves `--no-create`, supports a serial fallback, and rewrites fragile generated setup on OpenVMS/GNV. |
| `LS_CHECK_CFLAG` | Temporarily tests a candidate C flag with `-Werror`, caches the answer, and runs the selected action. | Restores the caller's `CFLAGS`; acceptance of this specimen is narrower than validation of every source file. |
| `LS_CHECK_LDFLAG` | Temporarily appends a candidate linker flag, performs a link test, caches the answer, and runs the selected action. | Restores `LDFLAGS`; the call site owns what to enable after success. |

### Parallel probes preserve a serial result boundary

The header and function macros first separate cached answers from work still needed. Each uncached probe gets its own directory and `conftest` files, preventing parallel probes from overwriting one another's source and result files. Workers write a small `conftest.out` result. A bounded batch is waited for before more work proceeds; this is batch scheduling, rather than a persistent worker pool.

After workers complete, the parent walks the original list in order, prints results, updates the usual `ac_cv_header_*` or `ac_cv_func_*` cache entries, emits `HAVE_*` definitions, and applies success/failure actions. Serial result import avoids concurrent modifications to the shared configuration definitions. The macros also provide `AH_TEMPLATE` entries so `autoheader` knows the generated definitions.

Function probes need additional care. They check whether `-fno-builtin` is accepted, optionally apply it, suppress the supported builtin-declaration diagnostic, and use a controlled declaration to avoid a compiler builtin standing in for the linkable symbol. They reject libc `__stub_*` markers. This makes the probe about the function symbol available to this build, rather than whether the compiler happens to recognize a familiar name.

The header macro uses `ac_fn_c_try_cpp`; the function macro uses `ac_fn_c_try_link`. Those generated Autoconf helpers and the quoting around embedded C/shell text are part of the maintenance boundary. An Autoconf upgrade deserves review of the emitted shell, not just confirmation that the M4 source still expands.

### Output generation and OpenVMS adaptation

`AC_OUTPUT_PARALLEL` saves the caller's `no_create` choice, prevents ordinary `AC_OUTPUT` from immediately running `config.status`, and then invokes the generated script itself when creation was requested. It detects `--jobs` support by scanning the generated script and, if needed, its help output. The macro does not assume that every Autoconf-generated `config.status` accepts parallel jobs.

On OpenVMS/GNV it restores the build directory, forces serial output, and transforms generated `config.status` fragments whose exit traps and compound redirections interfere with temporary AWK files. Cleanup is moved to an explicit normal-exit path. These changes are part of the wrapper around output generation, beyond simply supplying a worker count. Their exact matching depends on the generated shell form, so the [OpenVMS compatibility checks](../misc/platforms/openvms.md) matter when updating the generator.

## Regeneration and review

Use the project-local tool versions when regenerating. A full macro change normally requires `autoreconf -fi` with `.local/bin` first in `PATH`, followed by inspection of `aclocal.m4`, `configure`, `config.h.in`, affected `Makefile.in` files, and copied support files. Do not edit `configure` as the sole source of a durable macro change. Ordinary users running the prepared `configure` still do not need the macro generators.

Review the definition, call site, generated shell, and consumer of each result. For probe changes, check the actual compiler command and `config.log`; for scheduling changes, check cache reuse, worker isolation, and fallback behavior; for a new feature, compare the Meson result and public/generated outputs as well. A faster configure run is only useful if the resulting feature decisions still describe the intended toolchain.
