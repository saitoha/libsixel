# Test TODOs

## CLI helper coverage (converters/cli.c)
- Covered by new TAP unit tests in `tests/cli/t` using `test_xxxx_*.t` naming.
  - `test_0023_cli_token_is_known_option.t`: exercises short/long options,
    value-bearing tokens, bare hyphens, long-name overflow, and unknown tokens
    while checking `out_short_opt` resets.
  - `test_0024_cli_option_requires_argument.t`: verifies optstring parsing for
    required/optional/none cases using `"a:b::c"`.
  - `test_0025_cli_guard_missing_argument.t`: covers missing-argument
    reporting, leading-dash allowances, and optind rewind when a candidate
    argument is an option.

## sixel2png option handling
- Add integration TAP tests under `tests/cli/t` for `sixel2png` option flows with `test_xxxx_*.t` naming.
  - `test_0026_sixel2png_version_help.t`: run `-V` and `-H`; expect exit code 0 and version/help header on stdout.
  - `test_0027_sixel2png_missing_args.t`: run `sixel2png -i`; expect non-zero exit and stderr mentioning `--input` missing argument.
  - `test_0028_sixel2png_unknown_option.t`: run `sixel2png --unknown`; expect non-zero exit and "unknown option" message.
  - `test_0029_sixel2png_invalid_decoder_value.t`: run `sixel2png --similarity=invalid dummy.six`; expect `SIXEL_BAD_ARGUMENT` path with hint about bad similarity value.
  - `test_0030_sixel2png_default_output.t`: run `sixel2png -i dummy.six`; expect `dummy.png` created when `-o/--output` omitted.

## GDK Pixbuf loader robustness
- Covered by GLib unit tests in `tests/gdk-pixbuf-loader` using `test_xxxx_*.c` naming.
  - `test_0001_incremental_load.c`: feeds the tiny SIXEL sample in chunks and asserts
    prepared/updated callbacks, image dimensions, and successful completion.
  - `test_0002_corrupt_data.c`: provides clearly invalid SIXEL text, expects stop_load
    failure with `GDK_PIXBUF_ERROR_CORRUPT_IMAGE`.
  - `test_0003_propagate_error.c`: exercises `sixel_pixbuf_propagate_error` via the
    testing wrapper to confirm error domain, codes, and message prefix.
  - `test_0004_context_free.c`: obtains a loader context and frees it through the
    testing wrapper (plus NULL), ensuring no crash on cleanup.
