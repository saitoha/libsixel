# DRCS-SIXEL output

`img2sixel -@` / `--drcs` converts an image into downloadable character glyphs using the experimental DRCS-SIXEL extension. The terminal registers the image as a grid of cell-sized glyphs; an application can then display or redisplay that image by writing the corresponding characters. This is useful for text-oriented screen management, where moving, wrapping, or repainting characters is easier to integrate than managing a separate graphics surface.

Two outputs have different jobs: the **definition stream** loads the image glyphs into the terminal, and the **character stream** places those glyphs in text cells. The `PATH` field of `--drcs` selects where the character stream goes. It does not select where the image definition goes, and an omitted `PATH` does not automatically display the image.

## Protocol and receiver requirements

DRCS means *Dynamically Redefinable Character Set*. DEC's [DECDLD](https://vt100.net/docs/vt510-rm/DECDLD.html) command downloads soft character shapes. DRCS-SIXEL extends that mechanism to use a color SIXEL image as a source of glyph tiles. Unicode mapping is a further extension that makes those glyphs addressable through private-use characters.

A receiver needs the DRCS-SIXEL extension, the selected character mapping, and compatible character widths. Ordinary SIXEL support or traditional monochrome DEC soft-font support alone is insufficient. The [DRCS-SIXEL v2 specification](https://github.com/arakiken/mlterm/blob/master/drcssixel/DRCS-SIXEL-v2) and [v3 specification](https://github.com/arakiken/mlterm/blob/master/drcssixel/DRCS-SIXEL-v3) describe the extended wire format. mlterm and RLogin are the principal implementations discussed by those specifications; support must be checked against the receiver version and mapping mode actually in use. The CLI does not negotiate that capability.

The [project history](../project-history.md#drcs-sixel-and-unicode-plane-16) separates the 2012 RLogin image-glyph extension and drcsterm Unicode mapping, the 2017 v2 specification work, and the 2025 v3 proposal from their later releases. `MMV=0` in this CLI means ISO/IEC 2022 character output, not a Unicode mapping specification named “version 0”.

## Load, display, and replay

Run these commands from a terminal that provides usable pixel geometry and supports the selected DRCS mapping:

```sh
# Define the image and immediately write its display characters to stdout.
img2sixel -7 --drcs=2:1:- image.png

# Define the image on the terminal, saving only its display characters.
img2sixel -7 --drcs=2:1:tiles.txt image.png

# Later, display the already registered glyphs at a text position.
printf '\033[2;5H'
cat tiles.txt

# Save both parts separately; a controlling terminal is still required.
img2sixel -7 --drcs=2:1:tiles.txt -o image.drcs image.png

# Load definitions first, then display the characters.
cat image.drcs
cat tiles.txt
```

The default mapping revision is 2. A v3-capable receiver can instead use `--drcs=3:1:-`. The current v1/v2 emitter enables mode 8800 but does not reset mode 8801: after a previous v3 session, the application may need `printf '\033[?8801l'` before selecting v2. The application owns mapping-mode setup and restoration; the converter does not save and restore the receiver's prior modes.

Neither file is a standalone portable picture. `tiles.txt` needs the matching definitions and mapping mode in the same terminal; it can look like missing glyphs in an ordinary editor. `image.drcs` is a terminal font-definition stream rather than an ordinary `DCS ... q` SIXEL image for `sixel2png`. After a reset or glyph redefinition, reload the definitions before reusing saved characters.

## Argument syntax and output routing

```text
-@ MMV:CHARSET:PATH
--drcs=MMV:CHARSET:PATH
```

The CLI requires an argument. Within it, omitted or empty fields use defaults: mapping 2, starting charset 1, and no character output. Thus `--drcs=2`, `--drcs=2:1`, and `--drcs=::` all request definitions without a display-character stream. Use an explicit argument such as `2:1:-` for immediate display.

| Field or option | Meaning |
| --- | --- |
| `MMV` | Select ISO/IEC 2022 output (`0`) or Unicode mapping `1`, `2`, or `3`; default `2`. |
| `CHARSET` | One-based starting charset number in libsixel's selected mapping. It is not a Unicode code point, a literal designation byte, a terminal macro ID, or the number of tiles. Default `1`. |
| Empty/omitted `PATH` | Suppress character emission. The definition stream is still produced. |
| `PATH=-` | Write display characters to standard output, regardless of `-o`. |
| Other `PATH` | Open that path for writing display characters, creating or truncating it. Everything after the second colon is the path, so later colons belong to the path. |
| `-o FILE` | Route image definitions to `FILE`; without it, definitions go to stdout. It does not redirect `PATH=-`. |

Keep a named character file separate from the definition file. With default stdout and `PATH=-`, the converter writes a definition followed by its character grid to the same stream. With `-o image.drcs` and `PATH=-`, stdout contains only the character grid, so the saved definition must be loaded before that grid can display the intended image.

Repeating `--drcs` replaces the mapping, starting charset, and character sink, using defaults again for omitted fields. A named sink is opened and truncated while applying the option, before image conversion succeeds; a later option or conversion error does not undo that filesystem effect. The setting applies to subsequent frames and input images in that encoder instance, but it does not allocate a new charset automatically for each image.

## Mapping revisions

| `MMV` | Accepted starting `CHARSET` | Representation | First character for charset 1 |
| --- | --- | --- | --- |
| `0` | `1..126` | ISO/IEC 2022 designation and shift controls followed by glyph bytes. The current implementation has defects described below. | Byte `0x20` after designation. |
| `1` | `1..63` | Legacy Unicode mapping; four-byte UTF-8 characters. | `U+104020`. |
| `2` | `1..158` | Extended mapping using the 94/96-character-set namespaces; four-byte UTF-8 characters. | `U+103020`. |
| `3` | `1..697` | Sequential Plane 16 mapping in groups of 94; four-byte UTF-8 characters. | `U+100000`. |

These are parser bounds on the *starting* charset, not a guarantee that an arbitrary image fits within the remaining glyph space. The emitter does not preflight the complete tile count against mapping capacity or the terminal's font storage.

For v2, charsets 1–79 select designation final bytes `0x30..0x7e` with `Pcss=0`, and charsets 80–158 select the same finals with `Pcss=1`. Charset 1 therefore uses `SP 0`; charset 80 uses that same designation with the 96-character-set parameter. The UTF-8 sequence starts at `U+103020` or `U+1030A0`, respectively. The current v2 enumeration advances through 96 tile positions per designation, then advances the charset.

For v3, starting charset `S` maps its first tile to `U+100000 + (S - 1) * 94`, and later tile code points are consecutive. Its designation has `SP`, an additional intermediate byte, and a final byte. At `S=1` these are `SP SP @`. The application and terminal must interpret the selected private-use characters as single-cell image glyphs; a text layer that gives them two columns breaks the grid.

## Cell geometry and tile order

The converter opens `/dev/tty` and calls `TIOCGWINSZ`. It requires nonzero character rows/columns and pixel totals greater than those row/column counts. It computes integer cell dimensions as follows:

```text
cell_width  = ws_xpixel / ws_col
cell_height = ws_ypixel / ws_row
tile_columns = ceil(encoded_image_width  / cell_width)
tile_rows    = ceil(encoded_image_height / cell_height)
```

The encoded image dimensions are those after crop/resize preprocessing. `-w` and `-h` change the image size; they do not override the DRCS cell-geometry query. There is no automatic 8×16 fallback. A controlling TTY is required even when both outputs are files, and a TTY that supplies only rows/columns is insufficient. Builds without the applicable `TIOCGWINSZ` path, including the Emscripten branch, return an unsupported-operation status. Cell dimensions are cached in the encoder, so a terminal font/geometry change during its lifetime is not automatically re-queried.

For a 17×17 image on 8×16 cells, the result is three columns by two rows. Tile assignment runs from left to right, then top to bottom; the right and bottom tiles cover the partial edges:

```text
Image tile order:      v2 / charset 1 character grid:
+---+---+---+          U+103020  U+103021  U+103022  LF
| 0 | 1 | 2 |         U+103023  U+103024  U+103025  LF
+---+---+---+
| 3 | 4 | 5 |
+---+---+---+
```

The character stream contains a line feed after every row, including the last. It does not contain a complete layout manager: the caller controls cursor position, wrapping, margins, and line-feed handling. Edge-cell rendering and transparency still depend on the receiver. Large grids can wrap or scroll, and text applications must preserve the character order and cell width.

## Definition stream

For v2 charset 1, 7-bit controls, and 8×16 cells, the outer sequence is:

```text
ESC [ ? 8800 h
ESC P 1;0;0;8;1;3;16;0 { SP 0
    SIXEL raster attributes, palette definitions, and bitmap body
ESC \
```

The notation separates fields for readability; only the displayed `SP` is a designation space. The `8` and `16` fields specify cell dimensions, `3` selects the SIXEL image-glyph extension, and the final numeric field is `Pcss`. The ordinary inner SIXEL DCS envelope and its `q` parameter header are suppressed, while raster dimensions, palette commands, and bitmap data remain. This optional-header form is part of the extension specification.

For v3 charset 1, the prefix becomes `CSI ?8800;8801h`, the designation becomes `SP SP @`, and `Pcss` remains zero. `-8` uses the C1 CSI/DCS/ST bytes `0x9b`, `0x90`, and `0x9c`; the display characters for mappings 1–3 remain UTF-8. `-7` is useful when the transport is expected to carry escape sequences and UTF-8 without raw C1 bytes.

`CHARSET` changes the designation and character mapping. It does not change the fixed leading font/erase parameters or reserve storage on behalf of an application. Defining another image at overlapping glyph positions can change what previously written characters display. The current converter provides no glyph allocator, eviction notification, or automatic assignment of disjoint charsets to successive images.

## Related options and reuse mechanisms

| Feature | Relationship to DRCS output |
| --- | --- |
| `-u` / `-n` terminal macros | Explicitly rejected in combination with DRCS. Macros store terminal input and replay it by ID; DRCS registers glyphs that are displayed as characters. |
| Loader choice, `-S`, `-T`, `-l`, `-g` | Loading, frame selection, and host pacing still run. Animated input redefines glyphs and can emit character grids per frame; it does not preload an autonomous terminal animation. Use `-S` for a reusable still image. |
| Crop/resize, quantization, palette, dithering | Still process the image body before the terminal divides it into cells. DRCS does not remove quantization loss or create vector glyphs. |
| `-A` / `-B` | Alpha and background processing still matter, but the ordinary SIXEL `P2` header is suppressed. An explicit opaque background with `-Acomposite -B...` gives a simpler baseline than relying on receiver-specific transparent glyph behavior. |
| `-Z delta` / transparent offsets | The ordinary retained-display-plane `P2=1` contract is not established by DRCS output. The v2 extension specification does not support that preservation meaning. Treat these as unverified combinations, not a supported DRCS optimization. |
| `-O` / high-color output | Separate SIXEL dialect and receiver assumptions remain. In particular, `-O` normally signals OR behavior in the suppressed `P2` header. Acceptance of an option combination is not evidence that the glyph receiver can interpret it. |
| Multiple images or repeated playback | Each definition starts from the configured charset again. The converter does not preserve earlier images by choosing new slots. |

See [terminal macros](terminal-macros.md), [animation playback](animation.md), [alpha policy](../loader/alpha-policy.md), and [delta encoding](delta-encoding.md) for the surrounding contracts.

The transfer benefit comes from replaying the saved character grid after loading definitions once. For a grid of `C` columns and `R` rows, mappings 1–3 emit `4*C*R + R` bytes: four UTF-8 bytes per tile plus one LF per row, excluding cursor positioning. Re-running `img2sixel` sends definitions again and loses that reuse advantage. Receiver decoding, glyph caching, and text rendering costs are separate from transfer size; this byte count is not a measured speed claim.

## Current defects and support boundaries

The implementation is experimental. The following are current defects or gaps, not desirable protocol behavior or compatibility promises:

- For `MMV=0`, the common prefix formatter emits a literal `h` before DECDLD even though it emits no corresponding mapping-mode introducer. This can print a stray character and shift the starting position.
- The ISO/IEC 2022 helper initially emits an `ESC )` designation even when a 96-character starting set was requested; its initial designation does not follow the selected `Pcss` in that case. Its legacy rollover behavior should not be assumed to cover every mapping-0/1 boundary correctly.
- Switching from v3 to v1/v2 does not clear mode 8801, and finishing conversion restores neither mapping mode. Mixing revisions requires application-owned mode handling.
- The starting charset is validated, but the whole image's remaining glyph capacity is not. Large images or a late starting charset can exhaust the intended mapping space; v3 character generation has no final Plane 16 bound check.
- Receiving valid terminal geometry does not verify DRCS, mapping-version, color, transparency, or glyph-capacity support. No fallback to ordinary SIXEL is performed when DRCS is requested.

Use mappings 2 or 3 with a matching receiver and a bounded image as the baseline for integration work. Fixing the protocol-emitter defects requires dedicated output regressions rather than treating the current erroneous bytes as a stable wire contract.

## Implementation and validation

| Concern | Implementation |
| --- | --- |
| Parsing, defaults, charset bounds, and character-file lifetime | `sixel_encoder_apply_drcs_option()` in [`encoder.c`](../../src/encoder.c). |
| Geometry, macro conflict, outer-envelope integration, and per-frame output | `sixel_encoder_ensure_cell_size()` and frame encoding in [`encoder.c`](../../src/encoder.c). |
| Designations, mode prefixes, UTF-8/ISO/IEC 2022 character grids | [`drcs.c`](../../src/drcs.c). |
| Suppression of ordinary DCS/`q` while retaining raster attributes | [`sixel-writer.c`](../../src/sixel-writer.c). |
| Public option entry point | `SIXEL_OPTFLAG_DRCS` and `sixel_encoder_setopt()` in [`sixel.h.in`](../../include/sixel.h.in). The DRCS emit helpers are internal interfaces. |

There is currently no dedicated DRCS behavioral regression suite in `tests/`. The repository's [static checks](../testing/staticcheck.md) maintain documentation/help/man/completion consistency, but do not establish correct glyph output. No existing generic SIXEL or macro test should be treated as exact DRCS coverage.

A protocol-level check can run the CLI with a controlled pseudo-terminal, set both character and pixel geometry, and capture output without displaying it. Inspect definitions separately from character data: mapping-2/3 prefixes, starting charset, UTF-8 values, row breaks, rounding at partial cells, crossing a charset boundary, `-7`/`-8`, split output paths, and rejected macro combinations are distinct observations. A zero-pixel geometry control should reject the conversion. These checks establish emitted bytes and routing, not that a real receiver renders or retains the glyphs correctly.

Receiver validation additionally needs the actual terminal version, mapping modes, cell size, single-cell PUA widths, edge transparency, reuse after scrolling, slot collisions, and reset/redefinition behavior. Animation and stateful output combinations need their own validation rather than inheriting support from a successful still-image demonstration.
