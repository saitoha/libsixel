# Command-Line Interface Design Policy

## Scope

This document defines the design policy for libsixel command-line interfaces. The CLI-program layer includes the converters under `converters/` and specialized executable tools such as `lsqa` under `assessment/`; most detailed option contracts below concern `img2sixel` and `sixel2png`. A CLI is a public compatibility surface, not a thin test wrapper around the C library. Accepted spellings, defaults, precedence, operation order, diagnostics, output streams, and exit status are part of the interface contract.

The policy begins with established command-line guidance, then identifies the constraints specific to libsixel, explains the architecture selected from those inputs, and records why common alternatives were not selected. A new CLI decision is incomplete until both its rule and its rationale are documented.

## Baseline: established command-line guidance

No single external guide is binding on libsixel. POSIX optimizes for portable utility syntax, GNU extends that model for discoverability, and newer guidance treats a CLI as both a composable program interface and a human-facing user interface. libsixel takes compatible ideas from all three while retaining released behavior where a clean-sheet design would differ.

### POSIX utility syntax

The [POSIX Utility Syntax Guidelines](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap12.html) establish the traditional utility model: short option names, explicit option arguments, operands, `--` as the option terminator, and no implied semantic relationship among options merely because of their order. The accompanying [POSIX rationale](https://pubs.opengroup.org/onlinepubs/9799919799/xrat/V4_xbd_chap01.html) describes the goal as user portability and acknowledges that historical utilities retain exceptions.

libsixel adopts the recognizable option-and-operand model, short options, standard streams, and deterministic parsing. It does not claim strict POSIX syntax conformance: it also supports GNU-style long options, historical punctuation short flags, structured option arguments, and one documented order-sensitive geometry case. Those deviations exist for compatibility or to represent image-processing policy without exhausting the top-level namespace.

### GNU command-line conventions

The [GNU Coding Standards for command-line interfaces](https://www.gnu.org/prep/standards/html_node/Command_002dLine-Interfaces) recommend long names equivalent to short options, consistent common names, explicit output options, and standard `--help` and `--version` actions. These conventions add discoverability without discarding efficient short forms.

libsixel adopts paired short and long top-level options, `--help`, `--version`, explicit input and output controls, and familiar names where compatibility permits. It retains `-H` for help because `-h` already means image height, and it retains non-alphanumeric historical short flags because those character identities are published through the C API. Preserving those meanings is less surprising than retrofitting superficial conformance that would break existing commands.

### Human-facing CLI guidance

The open [Command Line Interface Guidelines](https://clig.dev/) emphasize that Unix composability and human usability can coexist: primary or machine-readable output belongs on standard output, messages belong on standard error, help and examples should make features discoverable, invalid input should be explained, and a likely correction should be suggested rather than silently executed.

libsixel adopts that separation of streams, contextual help, human-readable diagnostics, stable code-oriented diagnostics, validation, and deterministic suggestions. It does not make every modern recommendation a compatibility mandate. In particular, the converters do not become a subcommand suite or acquire implicit configuration files merely because those patterns work well for stateful multi-operation products.

## Product constraints that shape the design

libsixel is not a clean-sheet CLI. Five constraints determine which general conventions fit:

1. `img2sixel` and `sixel2png` already name two stable, opposite conversion directions and are widely usable as single-purpose pipeline stages.
2. Top-level short-option characters became public [`SIXEL_OPTFLAG_*`](../../include/sixel.h.in) identifiers consumed by library `setopt` functions, so converter syntax and the public C API share compatibility history.
3. Encoding is a staged transformation—loading, normalization, palette construction, palette application, and SIXEL serialization—rather than an arbitrary sequence of user-defined image operations. The [encoding pipeline](../functionality/encoding-pipeline.md) owns that stage model.
4. Each stage now has multiple algorithms and policy axes. A wholly flat option namespace cannot keep growing without collisions, opaque punctuation, or duplicated names.
5. The tools serve interactive terminal users, shell pipelines, automated quality tests, cross-platform builds, and library embedders. Human convenience cannot make byte streams or scripted failure handling unstable.

These constraints lead to a hybrid interface: conventional executables and top-level options, typed structured arguments for subsystem policy, a fixed semantic pipeline, explicit environment defaults, and separate human and machine-oriented diagnostic modes.

## Decision summary

| Concern | Selected design | Reason | Accepted cost and mitigation |
| --- | --- | --- | --- |
| Program shape | Keep `img2sixel` and `sixel2png` as separate directional utilities. | A command name states the conversion direction and composes directly in a pipeline. | Shared controls must remain synchronized across two binaries; registries and static checks enforce that synchronization. |
| Top-level syntax | Give every public top-level option a short and a long form. | Long names teach; short names preserve established interactive and C API usage. | The short namespace is finite; independent additions require review and subsystem-specific growth moves to typed suboptions. |
| Complex settings | Use `BASE[:SUBOPTION...]` with typed long and compact suboption forms where one setting owns a family of subordinate policy. | The base selects an algorithm or policy family while subordinate controls stay in their owning namespace. | The colon grammar is project-specific; help, manuals, completion, exact key matching, and descriptive long forms make it learnable. |
| Processing order | Treat options as configuration for a fixed pipeline, except that crop and resize preserve their relative CLI order. | Most stages have one valid architectural order; crop and resize are non-commutative geometry operations for which both orders are useful. | The exception must remain narrow, documented, and directly tested. |
| Configuration | Give every configurable public option and suboption an explicit environment form, resolve command line before environment and built-in defaults, and do not load an implicit config file. | Interactive commands, wrappers, CI, and launchers can express the same setting without inventing parallel configuration semantics. | Inherited environments are ambient state; help must expose the mapping and reproducible commands should state material choices explicitly. |
| Diagnostics | Offer human and stable code modes, keep suggestions diagnostic-only, and send messages to standard error. | Interactive correction and automation need different presentation stability without different error semantics. | Two modes and configurable suggestions increase implementation work; shared registries and focused tests prevent divergence. |
| Abnormal termination | Let adopting CLI executables install a narrow, policy-controlled abort trace only for otherwise unhandled `SIGABRT`. | Immediate failure context helps reports while preserving sanitizer, debugger, and core-dump ownership. | In-process crash reporting is best-effort and may disclose symbols or addresses; the dedicated policy documents the current consumer set and these limits. |

## Alternatives not selected

### Strict POSIX short options only

A strictly POSIX surface would maximize parser portability but remove descriptive long names and reject already-published punctuation flags. libsixel instead provides its own `getopt_long()` fallback on platforms that need one and treats compatibility with released CLI and C identifiers as the stronger requirement. New deviations still need a semantic reason; history is not permission to invent arbitrary syntax.

### One flat set of long options

Flattening every control into names such as `--quantize-kmeans-init-type` would be easy to parse but would repeat subsystem names, flood help output, and leave no credible one-character partner for each new top-level setting. More importantly, it would obscure that some controls are valid only for a particular base algorithm. Typed suboptions keep validation and discovery in the active domain, while a descriptive long spelling remains available for scripts and documentation.

This explains why the current structure remains internally coherent; it does not establish that the structure was the best clean-sheet CLI design. In hindsight, if the eventual number and depth of settings had been foreseeable in 2014, libsixel would have made descriptive long options the primary interface and reserved short forms for the most frequent operations. Interix provided `getopt()` but not `getopt_long()`, yet that limitation did not make a long-option-first design impossible: with the expected scale visible, implementing a compatibility `getopt_long()` at that point would have been a reasonable engineering choice. The later MSVC and OpenVMS fallback demonstrates the shape of the implementation that could have been introduced earlier, although it does not erase the cost and uncertainty such work would have carried in 2014.

The present suboption form acquires a rational scoped grammar once learned, but the additional colon syntax and context-dependent compact letters create genuine operational surprise for first-time and occasional users. On that user-experience criterion, it is difficult to call the result the CLI-design optimum. The project retains the structure because its released spellings, environment mappings, validation scopes, manuals, completion, and C-facing option history are now compatibility surface. Future work should improve discovery and avoid unnecessary new nesting; consistency with an imperfect historical structure is evidence to weigh, not an automatic design decision.

### One umbrella executable with subcommands

A `sixel encode` / `sixel decode` command could share global help and configuration, and subcommands are valuable when a product has many nouns and actions. libsixel has two established, stateless conversion directions whose executable names already describe their work. Replacing them with subcommands would lengthen common pipelines, complicate packaging and dispatch, and break existing scripts without simplifying the underlying encoder policy. An additive umbrella tool could be considered separately if future workflows justify it, but it must not silently replace the directional utilities.

### An order-sensitive image-operation language

[ImageMagick command-line processing](https://usage.imagemagick.org/basics/) intentionally executes image operators in command order because it represents arbitrary reads, transforms, image lists, stacks, and compositions. libsixel does not expose that general image-programming model. Its options select policies for a known conversion pipeline, so making every flag execute at its textual position would expose internal staging, multiply invalid combinations, and make equivalent command lines unexpectedly order-dependent.

Crop and resize are the deliberate exception because the two operations do not commute and both coordinate interpretations are useful. That exception does not turn palette construction, dithering, colorspace selection, diagnostics, or output policy into ordered operators.

### Implicit configuration files

A user or project config file would reduce repetition but introduce filesystem lookup rules, directory-dependent behavior, format migration, and another precedence layer. Image conversion commands are usually expected to be reproducible from their invocation and input. Environment variables provide process-scoped defaults without repository state, and `--env NAME=VALUE` can make the environment form explicit for one invocation. This mechanism is for settings, not secrets; sensitive material should not be placed in command arguments or public configuration environments.

### Silent correction of invalid input

Automatically accepting a likely typo feels convenient once but silently creates aliases, makes future names harder to add, and can execute a materially different conversion than requested. libsixel separates matching from suggestions: only declared exact or prefix rules can succeed, while fuzzy candidates explain a failure and leave the command unsuccessful.

## Program and data model

### Directional utilities and operands

`img2sixel` owns raster or supported source loading followed by SIXEL encoding. `sixel2png` owns SIXEL decoding followed by PNG output. Keeping those directions separate makes the common operation visible in the executable name and keeps each tool useful as one stage in a larger shell pipeline.

`lsqa` is a separate quality-assessment CLI rather than a third conversion direction. Program-level facilities may be reused across those executables, but inclusion in the CLI layer does not imply that every facility is enabled by every program. Each shared control must identify its actual consumers; aborttrace, for example, is currently installed by the two converters and not by `lsqa`.

Input and output are explicit options where needed, while `-` represents standard input or standard output in documented file positions. The CLI also has typed pseudo targets such as `clipboard:` and `png:clipboard:` when a path position needs to name a non-filesystem transport. These are explicit sentinels rather than magic filename guessing, so scripts can determine which transport is requested from the command text.

### Fixed pipeline order

For most options, argument order records configuration precedence but does not schedule processing. Loader policy still belongs to loading, quantizer policy to palette construction, lookup and diffusion to palette application, and encoding policy to serialization regardless of where their flags appear. A fixed order keeps the CLI aligned with the library architecture, permits planning and parallelism without changing semantics, and prevents an implementation refactor from becoming a user-visible reordering.

When options interact, the contract must state whether an explicit choice wins, whether the combination is rejected, or whether a documented fallback applies. Do not use argument position as an undocumented conflict resolver.

### Crop and resize order exception

`img2sixel` preserves the relative order of `-c` / `--crop` and the resize dimensions `-w` / `--width` and `-h` / `--height`:

```sh
# Crop in source coordinates, then resize the cropped result.
img2sixel -c 320x200+40+20 -w 640 input.png

# Resize first, then crop in resized coordinates.
img2sixel -w 640 -c 320x200+40+20 input.png
```

Width and height configure one resize operation; swapping `-w` and `-h` does not create two resize passes. When crop or dimension options repeat, the last value in each family supplies the effective geometry, and the last occurrence across the crop and resize families determines which of those two operations runs first. This rule gives users both useful coordinate spaces without adding a separate ordering flag.

The exception is justified because crop and resize are visibly non-commutative: cropping before scaling selects source pixels, while cropping after scaling selects pixels in the resized image. The planner records the chosen dependency edge so optimized and serial execution preserve the same result. No other option becomes order-sensitive by analogy; a new exception requires a user-visible non-commutative operation, explicit documentation, and coverage for both orders.

## Option architecture

### Two levels of control

Top-level options select major operations, independent pipeline stages, or broadly applicable behavior. Typed suboptions group controls owned by a loader, quantizer, diffusion method, lookup method, runtime policy, diagnostics policy, or another named subsystem. Ownership follows the user's mental model and the processing stage, not the availability of a convenient short character.

For example, `-Q kmeans:inittype=pca` and its compact spelling `-Q kmeans:Ipca` select the same k-means initialization policy without allocating another top-level flag. The [converter suboption architecture](suboptions.md) defines the complete grammar, registry metadata, scope rules, typed parsing, environment-variable parity, precedence, diagnostics, and implementation path. It is principally the structured option system of `img2sixel` and `sixel2png`; `lsqa` does not adopt that architecture generally, although its `-d` dequantize option deliberately reuses the decoder parser and its limited suboptions.

### Top-level short and long forms

Every public top-level option of `img2sixel` and `sixel2png` must have both a one-character short form and a long form with the same argument shape. The long form is the discoverable, self-explanatory spelling. The short form is the stable dispatch identity and efficient interactive spelling.

During libsixel's initial development in 2014, long options mapped back to the character used by their short option. Interix later exposed an environment with `getopt()` but not `getopt_long()`, making the short form the only portable option interface at the time. In 2015 those characters became public `SIXEL_OPTFLAG_*` macros accepted by library `setopt` functions. Later MSVC and OpenVMS ports added the project's [`getopt_long()` fallback](../../converters/getopt_stub.h), removing the parser limitation but not the published character contract.

As the interface grew, letters and digits ceased to be enough, and released flags consumed punctuation such as `~`, `=`, and `+`. This is a historical portability deviation, not a preferred source of future names. Typed suboptions let one stable top-level operation grow without allocating increasingly opaque punctuation. They must not be used merely to hide an independent stage under an unrelated option.

The one-character namespace is converter-local because encoder and decoder flags are parsed by different programs. If a setting is owned by an existing structured option, add a suboption. If it is independent, allocate a reviewed top-level identifier; lack of a convenient character is evidence that the interface needs design work, not permission to misclassify the feature.

### Suboption forms and names

Use suboptions for policy specific to a named subsystem. A public suboption has a typed domain, one canonical long name, one uppercase compact name, and an explicit environment representation. Long `name=value` keys are exact, while the compact `Kvalue` form is a separately registered spelling rather than a key abbreviation. Names describe user-visible behavior in established libsixel terminology rather than a temporary function, data structure, dependency, or optimization. The detailed naming and parsing contracts belong to the [suboption architecture](suboptions.md).

### Internal controls

Fault injection and test-only implementation switches are not public options or environment contracts. Their names use the `_SIXEL_TEST_*` namespace and do not appear in public help, manual pages, shell completion, or language bindings. This boundary prevents a test seam from becoming a compatibility promise or an accidental production control.

## Defaults and configuration

Every configurable value has one identifiable default and a documented precedence order. Unless an established interface defines another order, use:

1. command-line value;
2. environment value;
3. built-in default.

The command line wins because it is the most local and visible expression of intent. Environment variables are useful for wrappers, CI, platform launchers, and user defaults, but inherited state must not override an explicit invocation. Built-in defaults make an unconfigured command usable and provide the final deterministic fallback.

Every public option or suboption that represents a configurable setting has a corresponding environment form. This is a primary design rule, not an optional convenience: a new setting is incomplete until both channels reach the same typed state. Pure actions such as help and version and positional input or output targets are not settings. The environment name is part of the public interface and is listed by help and the manual rather than inferred by mechanically capitalizing the CLI name. Multiple CLI routes may share one environment variable when they intentionally configure the same state. Registry-backed converter settings are mechanically audited today; other CLI surfaces must be reviewed against the same rule until equivalent structural coverage exists.

`--env NAME=VALUE` supplies an environment-form setting for one converter invocation. It is not a second setting or a higher precedence layer: a corresponding explicit CLI setting still wins. This form is valuable to launchers and cross-platform tests that cannot rely on shell-specific leading assignment syntax.

An empty public environment value is equivalent to an unset value unless an older documented contract explicitly requires otherwise. Public boolean CLI values accept only `0` and `1`; new boolean environment contracts should do the same. If a released environment parser retains its default on invalid input, as aborttrace does, that exception must be documented and tested rather than generalized.

An explicit value equal to the default remains distinct from an unset value when downstream behavior depends on whether the user made a choice. Reproducible reports and benchmarks should spell out every material non-default and should record relevant inherited environment values.

## Parsing and validation

Parsing must reject malformed input before conversion uses partially initialized state. Apply consistent typed rules for choices, integers, sizes, floating-point values, paths, lists, and structured arguments. Reject trailing garbage, overflow, underflow, out-of-range values, invalid combinations, and unavailable backend choices deterministically.

Validate at the layer with enough context to identify the responsible option, active base, invalid token, and valid domain. A low-level parser may detect a bad byte sequence, but the CLI-facing layer owns a message that tells the user which setting to correct.

### Choice values and prefix abbreviations

Choice values may use case-sensitive prefix matching inside a declared finite domain. For example, `-s ave` resolves to `average`, while `-d st` is rejected because it matches both `stucki` and `stbn`; `scan=ser` may abbreviate the value `serpentine`, but `sca=serpentine` may not abbreviate the suboption key `scan`. Exact values, aliases that share one semantic value, environment matching, ambiguity, and compatibility constraints are defined by the [choice prefix-matching policy](prefix-matching.md).

## Diagnostics and suggestions

Diagnostics are designed output, not incidental parser text. Human-readable mode should help a user correct an invocation; code mode should retain a stable error category suitable for automation. Both modes represent the same success or failure and must not select different conversion behavior.

- Identify the option, active subsystem, and invalid value precisely because correction depends on that context.
- Send diagnostics to standard error because standard output may carry SIXEL, PNG, JSON, or another machine-readable result.
- Keep prefix, fuzzy-name, and path suggestions independently controllable because filesystem disclosure and interactive verbosity have different deployment costs.
- Treat suggestions as explanation only because silently applying a guess changes semantics and creates an accidental alias.
- Avoid leaking uninitialized memory, unrelated candidate names, or platform-specific paths because helpful diagnostics must not become an information-disclosure channel.
- Keep library and standalone-converter defaults separate because an embedding application owns its own user experience.

An ambiguous accepted prefix is rejected with an `ambiguous prefix` diagnostic and may list the matching names. An unknown value or key may add `Did you mean:` candidates, but the command still fails rather than silently applying a guess. These facilities are independently controlled by `prefix_suggestions`, `fuzzy_suggestions`, and `path_suggestions`; their defaults, ranking algorithms, privacy and performance effects, diagnostic-mode behavior, and environment forms are defined by the [correction-suggestion policy](correction-suggestions.md).

### Abnormal-termination diagnostics

An in-process crash diagnostic has a narrower contract than an ordinary error: it must declare which failures it handles, defer to an existing sanitizer or debugger owner, avoid contaminating standard output, preserve platform termination behavior where possible, and admit that recovery and symbolization from a failing process are best-effort.

The [CLI-program abort-trace policy](abort-trace.md) applies those rules to otherwise unhandled `SIGABRT`. It distinguishes CLI-layer ownership from the current `img2sixel` and `sixel2png` consumer set and from the non-consuming `lsqa` executable, then defines the facility's purpose, build and runtime controls, installation timing, terminal recovery, output boundaries, platform limitations, performance and quality effects, representative output, comparable facilities, and reciprocal tests.

## Input and output contracts

- Keep SIXEL, raster, JSON, or other machine-readable output free of progress messages and diagnostic chatter because downstream programs consume those bytes.
- Send status and error diagnostics to standard error because users should still see them when standard output is redirected or piped.
- Define standard input and standard output behavior explicitly, including conflicts between binary input, terminal input, and option arguments, because implicit TTY behavior can hang automation.
- Treat file overwriting, output creation, partial-output cleanup, and broken pipes as designed behaviors because filesystem and pipeline side effects remain observable after failure.
- Keep help, version, and error exit statuses stable and scriptable because shell callers use status independently of human prose.

### Pseudo targets

`clipboard:` is a pseudo path accepted wherever a converter accepts a supported input or output target. As an input it reads a compatible image or text payload from the selected clipboard backend; as an output it publishes the generated payload. A documented format prefix can be combined with it, for example `png:clipboard:`, to select the clipboard representation.

The marker must terminate the operand and is interpreted as a pseudo target rather than a filesystem path or remote URL. It is distinct from the `-` standard-stream sentinel. Clipboard backend policy is configured by `-y` / `--clipboard-policy`, its typed suboptions, and corresponding environment variables. An explicit marker avoids probing the filesystem or guessing from content where an output should go.

## Compatibility and evolution

- Do not silently change a default because an omitted option is still user-visible behavior.
- Do not reuse an accepted flag, key, value, environment variable, or pseudo target for a different meaning because old scripts must not acquire new semantics.
- Do not remove a released spelling without an explicit deprecation and migration plan because aliases and abbreviations are interfaces once users rely on them.
- Keep library and converter compatibility separate because a CLI convenience must not silently change defaults for embedders.
- Prefer additive changes only when they do not make the interface less coherent; accumulation itself has a usability cost.

When behavior must change, document the old behavior, new behavior, rationale, migration path, affected interfaces, and release boundary. If a new choice would invalidate an accepted prefix or a new top-level option would force an opaque short flag, reconsider the grouping before publishing it.

## Documentation and review checklist

A public CLI change is complete only when its design, implementation, discovery surfaces, and evidence agree:

1. State the user problem and explain why it belongs in the CLI rather than only in the library or a separate tool.
2. Identify the owning pipeline stage or cross-cutting policy and explain why the setting is top-level or nested.
3. Compare the chosen form with relevant established conventions and repository alternatives; record both the benefit and accepted cost.
4. Define the short form, long form, type, default, environment name, precedence, repetition behavior, interactions, unavailable-platform behavior, diagnostics, output effects, and exit semantics.
5. Update help, manuals, shell completion, option registries, public headers when applicable, and subject documentation in the same change.
6. Add focused positive, negative, precedence, interaction, and output-integrity coverage, with reciprocal links when a durable policy document owns the contract.

The purpose of this checklist is not uniformity for its own sake. It keeps a local convenience from becoming an unexplained permanent compatibility burden.

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and a corresponding static check or test. The reciprocal `Policy:` reference in each check or test is enforced by `staticcheck-doc-test-links`.

| ID | Design contract | Static check or test |
| --- | --- | --- |
| CLI-01 | Every public top-level option has a one-character short form and a long form with the same argument shape. | [tests/_static/sh/staticcheck-suboption-registry.sh](../../tests/_static/sh/staticcheck-suboption-registry.sh) |
| CLI-03 | `img2sixel -H` and the manual expose the same top-level option declarations. | [tests/_static/sh/staticcheck-docs-help-vs-man.sh](../../tests/_static/sh/staticcheck-docs-help-vs-man.sh) |
| CLI-04 | The manual and Bash completion expose the same top-level option declarations. | [tests/_static/sh/staticcheck-docs-man-vs-bash-completion.sh](../../tests/_static/sh/staticcheck-docs-man-vs-bash-completion.sh) |
| CLI-05 | Public environment controls are represented in the generated help inventory. | [tests/_static/sh/staticcheck-docs-envvars-help-table.sh](../../tests/_static/sh/staticcheck-docs-envvars-help-table.sh) |
| CLI-06 | Internal test environment controls remain behind the internal environment interface. | [tests/_static/sh/staticcheck-src-no-direct-getenv.sh](../../tests/_static/sh/staticcheck-src-no-direct-getenv.sh) |
| CLI-10 | An explicit command-line value takes precedence over its environment default. | [tests/cli/options/matching/0246_option_matching_sampling_policy_env_cli_precedence.t](../../tests/cli/options/matching/0246_option_matching_sampling_policy_env_cli_precedence.t) |
| CLI-11 | Missing required arguments are detected before option dispatch. | [tests/cli/argument-shift/0007_cli_guard_missing_argument.t](../../tests/cli/argument-shift/0007_cli_guard_missing_argument.t) |
| CLI-13 | Explicit standard input and standard output form a working binary conversion path. | [tests/cli/core/0009_basic_stdin_stdout_map64.t](../../tests/cli/core/0009_basic_stdin_stdout_map64.t) |
| CLI-14 | `png:-` writes PNG data to standard output. | [tests/cli/core/0014_basic_png_stdout.t](../../tests/cli/core/0014_basic_png_stdout.t) |
| CLI-15 | The help command remains available. | [tests/cli/core/0001_help.t](../../tests/cli/core/0001_help.t) |
| CLI-16 | The version command remains available. | [tests/cli/core/0002_version.t](../../tests/cli/core/0002_version.t) |
| CLI-22 | `clipboard:` and `png:clipboard:` work as input and output pseudo targets. | [tests/io/clipboard/0002_clipboard_file_backend.t](../../tests/io/clipboard/0002_clipboard_file_backend.t) |
| CLI-23 | Typed scalar options and suboptions register environment names, and public environment controls remain synchronized with help. | [tests/_static/sh/staticcheck-suboption-registry.sh](../../tests/_static/sh/staticcheck-suboption-registry.sh), [tests/_static/sh/staticcheck-docs-envvars-help-table.sh](../../tests/_static/sh/staticcheck-docs-envvars-help-table.sh) |
| CLI-26 | `img2sixel` preserves both crop-before-resize and resize-before-crop planner order, and the two orders remain observably distinct. | [tests/loader/builtin/1513_loader_builtin_pal8_trns_clipfirst_order_preserved.t](../../tests/loader/builtin/1513_loader_builtin_pal8_trns_clipfirst_order_preserved.t) |

### Coverage boundary

Structural checks cover paired top-level forms, environment exposure, help, manuals, and completion. Behavioral tests cover representative precedence, binary I/O, and both geometry orders. Detailed suboption, prefix-matching, and correction-suggestion contracts and tests belong to their linked documents. Design judgment remains manual: tests cannot decide whether a name is clear, whether a new feature belongs in an existing subsystem, whether another order-sensitive exception is justified, or whether the compatibility cost of a new spelling is acceptable.
