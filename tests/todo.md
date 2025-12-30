# Test TODOs

## CLI helper coverage (converters/cli.c)
- Covered by new TAP unit tests in `tests/` using `test_xxxx_*.t` naming.
  - `0029_cli_token_is_known_option.t`: exercises short/long options,
    value-bearing tokens, bare hyphens, long-name overflow, and unknown tokens
    while checking `out_short_opt` resets.
  - `0030_cli_option_requires_argument.t`: verifies optstring parsing for
    required/optional/none cases using `"a:b::c"`.
  - `0031_cli_guard_missing_argument.t`: covers missing-argument
    reporting, leading-dash allowances, and optind rewind when a candidate
    argument is an option.

## sixel2png option handling
- Add integration TAP tests under `tests/` for `sixel2png` option flows with `test_xxxx_*.t` naming.
  - `0032_sixel2png_version_help.t`: run `-V` and `-H`; expect exit code 0 and version/help header on stdout.
  - `0033_sixel2png_missing_args.t`: run `sixel2png -i`; expect non-zero exit and stderr mentioning `--input` missing argument.
  - `0034_sixel2png_unknown_option.t`: run `sixel2png --unknown`; expect non-zero exit and "unknown option" message.
  - `0035_sixel2png_invalid_decoder_value.t`: run `sixel2png --similarity=invalid dummy.six`; expect `SIXEL_BAD_ARGUMENT` path with hint about bad similarity value.
  - `0036_sixel2png_default_output.t`: run `sixel2png -i dummy.six`; expect `dummy.png` created when `-o/--output` omitted.

## GDK Pixbuf loader robustness
- Covered by GLib unit tests in `tests/gdk-pixbuf-loader` using `test_xxxx_*.c` naming.
  - `0002_incremental_load.c`: feeds the tiny SIXEL sample in chunks and asserts
    prepared/updated callbacks, image dimensions, and successful completion.
  - `0003_corrupt_data.c`: provides clearly invalid SIXEL text, expects stop_load
    failure with `GDK_PIXBUF_ERROR_CORRUPT_IMAGE`.
  - `0004_propagate_error.c`: exercises `sixel_pixbuf_propagate_error` via the
    testing wrapper to confirm error domain, codes, and message prefix.
  - `0005_context_free.c`: obtains a loader context and frees it through the
    testing wrapper (plus NULL), ensuring no crash on cleanup.
