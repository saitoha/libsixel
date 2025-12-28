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
- Add small GLib/C tests in `tests/gdk-pixbuf-loader` with `test_xxxx_*.c` naming.
  - `test_0001_incremental_load.c`: feed minimal SIXEL bytes; call `sixel_pixbuf_begin_load` → `load_increment` (split chunks) → `stop_load`.
    - Expect both `prepared_func` and `updated_func`; resulting `GdkPixbuf` width/height match fixture; no `GError` set.
  - `test_0002_corrupt_data.c`: feed truncated/malformed SIXEL data through same sequence.
    - Expect failure with `GError` domain `GDK_PIXBUF_ERROR` and code `GDK_PIXBUF_ERROR_CORRUPT_IMAGE`.
  - `test_0003_propagate_error.c`: call `sixel_pixbuf_propagate_error` with `SIXEL_BAD_INPUT`, `SIXEL_BAD_ALLOCATION`, etc.
    - Expect domain `GDK_PIXBUF_ERROR`, correct enum codes, and messages including status name.
  - `test_0004_context_free.c`: call `sixel_pixbuf_context_free(NULL)` and double-free scenarios.
    - Expect no crash or double-free side effects.
