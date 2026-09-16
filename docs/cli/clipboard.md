# Clipboard input and output

The converters recognize `clipboard:` as a pseudo path. It names a clipboard transport rather than a disk file; `-y` / `--clipboard-policy` selects its backend. A format prefix such as `png:clipboard:` requests an image representation where the converter and backend support it. This is separate from network input and from the `-` standard-stream sentinel.

## Common workflows

```sh
# Read PNG data from the clipboard and display a SIXEL image.
img2sixel png:clipboard:

# Publish SIXEL text for another SIXEL-aware application.
img2sixel -o clipboard: image.png

# Decode SIXEL to a PNG clipboard image for an ordinary graphics application.
sixel2png -i image.six -o png:clipboard:

# Read SIXEL clipboard text and save a PNG file.
sixel2png -i clipboard: -o image.png
```

SIXEL text and a raster clipboard image have different consumers. Pasting SIXEL text into an ordinary image editor does not make the editor decode SIXEL. A clipboard format label also does not itself transcode bytes: conversion remains the converter's responsibility. In particular, do not infer a TIFF encoder from the clipboard backend's ability to carry TIFF data. For the supported PNG writer and `img2sixel` snapshot behavior, see [PNG writing](../writers/png.md).

Use an explicit image prefix for portable image input. An unprefixed input uses the converter's default request; some backends probe image representations anyway, while others select text. On macOS, the current reader probes PNG, TIFF, then a string. Linux only takes its PNG helper path for a PNG request. Consequently, the same unprefixed clipboard contents need not be selected identically on every OS.

The marker must be at the end of the operand. `clipboard:extra` is not the clipboard pseudo target. Its recognition and format prefix are handled by [`clipboard.c`](../../src/clipboard.c); supported input bytes then pass through the ordinary loader or SIXEL decoder.

## Backend policy and precedence

| Setting | Meaning |
| --- | --- |
| `-y system` | Use the compiled desktop clipboard implementation; this is the default. |
| `-y file:directory=PATH` | Use deterministic disk slots in an existing directory. This backend is useful for headless tests and integrations. |
| `SIXEL_CLIPBOARD_BACKEND` | Environment fallback for backend selection. The legacy environment spelling `fake` is also accepted as `file`. |
| `SIXEL_CLIPBOARD_FILE_DIR` | Environment fallback for the file backend's directory. |

Explicit CLI fields override their environment counterparts. The compact directory suboption is `:DPATH`. Configure the backend before performing clipboard I/O; these controls are process-level policy rather than a distinct clipboard per encoder object. See [suboption architecture](suboptions.md) for parsing and scope.

The file backend uses `image.bin` for PNG/TIFF requests and `text.bin` for text. These are two slots, not a general MIME database or a multi-item clipboard. A default text read tries `text.bin` first and falls back to `image.bin` if that slot is unavailable; an explicit image request does not fall back to text. Remove an obsolete slot deliberately when using this backend interactively. A write replaces the selected slot and does not synchronize with the desktop clipboard. The directory must already exist; the backend does not create a directory tree.

```sh
mkdir -p ./clipboard-store
sixel2png -y file:directory=./clipboard-store \
    -i image.six -o png:clipboard:
img2sixel -y file:directory=./clipboard-store png:clipboard:
```

## System implementations

| Build/platform | Mechanism and boundary |
| --- | --- |
| macOS AppKit | `NSPasteboard`; image input probes PNG then TIFF before string fallback. Output chooses the requested pasteboard representation. A legacy Carbon implementation is a separate build path. |
| Windows | Native clipboard API, registered image formats, and UTF-16 text conversion. Registered PNG availability is distinct from Windows bitmap/DIB availability; the backend is not a universal image-format converter. |
| Linux desktop | External `wl-copy`/`wl-paste` helpers, with `xclip` fallback. The Wayland path requires both helpers to be present. PNG and text have separate helper commands. |
| Build without a system backend | System clipboard operations are unsupported. The file backend provides a separate headless transport. |

Helpers being installed does not establish access to a desktop session. Display/session environment, clipboard ownership, and a matching payload still matter. The selected operation can fail even when the build reports clipboard support. A successful publication replaces clipboard contents; it is not an append operation or a history entry managed by libsixel.

The current converters stage clipboard payloads through temporary storage before or after conversion. This is not streaming frame delivery or a promise of zero-copy memory use. A desktop clipboard is also unsuitable as an animation clock: publishing a payload does not make the recipient replay the converter's host-side delays.

## Diagnostics and implementation

`-x human:trace_topic=clipboard_contract` exposes the selected backend and whether a directory is configured. It does not prove that a desktop recipient accepts the payload. The normal `img2sixel` exit mapping distinguishes clipboard failures from argument and other runtime failures; see [diagnostics](diagnostics.md).

Implementation: [`clipboard.c`](../../src/clipboard.c), [`clipboard_macos.m`](../../src/clipboard_macos.m), [`clipboard_carbon.c`](../../src/clipboard_carbon.c), and the pseudo-target/spool integration in [`encoder.c`](../../src/encoder.c) and [`decoder.c`](../../src/decoder.c).

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation | Owning test |
| --- | --- | --- |
| CLP-01 | File transport publishes a PNG signature and SIXEL text and permits the converter round trip. Its additional MS-SSIM check is only a quality floor. | [tests/io/clipboard/0002_clipboard_file_backend.t](../../tests/io/clipboard/0002_clipboard_file_backend.t) |
| CLP-02 | The environment accepts `file` and legacy `fake` case-insensitively. | [tests/io/clipboard/0003_clipboard_backend_environment.t](../../tests/io/clipboard/0003_clipboard_backend_environment.t) |

### Coverage boundary

The [clipboard suite](../../tests/io/clipboard/) and [CLI migration suite](../../tests/cli/options/migration/) contain further availability, directory, and precedence checks. File-backend success does not validate AppKit, Windows clipboard ownership, Wayland, X11, or the format preferences of another desktop application. Those need platform/session-specific integration checks.
