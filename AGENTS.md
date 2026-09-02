# AGENTS.md

## Scope

These instructions apply to the entire libsixel repository. User instructions
take precedence. Before making a non-trivial change anywhere in the repository,
read the project-wide engineering guide in [docs/AGENTS.md](docs/AGENTS.md).

## Project overview

libsixel is a C implementation of the DEC SIXEL terminal graphics format. It
provides an encoder and decoder library, command-line converters, optional image
loaders, image-quality assessment tools, language bindings, and integration
examples. The principal command-line programs are `img2sixel` and `sixel2png`.

The repository supports both Autotools and Meson. Keep source lists, generated
build inputs that are intentionally tracked, tests, and public interfaces in
sync across both build systems.

Important directories include:

- `src/`: core encoder, decoder, loader, quantization, and palette code.
- `include/`: installed public C headers.
- `converters/`: command-line programs, manuals, and shell completion.
- `assessment/`: `lsqa` and image-quality measurement code.
- `tests/`: functional, regression, quality, security, and static tests.
- `fuzz/`: fuzz targets, dictionaries, and related support.
- `amalgamation/`: single-file distribution support.
- `docs/`: durable project-wide engineering documentation.
- `.github/workflows/`: the GitHub Actions build, test, fuzz, and triage matrix.

## Working rules

- Keep C source lines within 80 columns and follow the style already used by
  the surrounding code.
- Use C99 with K&R brace style. Declare every local variable at function scope
  at the beginning of the function; do not mix declarations with statements.
- Read nearby comments before editing code. Correct stale comments and add
  explanatory comments when they materially reduce regression risk.
- Put `$TOP_SRCDIR/.local/bin` at the front of `PATH` when invoking Autotools,
  Meson, or related project-local build tools.
- Verify changes in proportion to their risk. For normal source changes, the
  local completion bar is `make staticcheck` followed by `make check`.

## Documentation

Use [docs/AGENTS.md](docs/AGENTS.md) as the index and maintenance policy for
the engineering documentation. The principal references are:

- [Functional overview](docs/functional-overview.md)
- [CLI design policy](docs/cli-design.md)
- [Quality measurement policy](docs/quality-measurement.md)
- [Testing guide](docs/testing.md)
- [CI architecture and design](docs/ci-design.md)

Keep `docs/` focused on durable project-wide guidance. Issue investigations,
one-off implementation notes, and task-specific reports belong outside the
tracked repository unless the user explicitly requests their publication.
