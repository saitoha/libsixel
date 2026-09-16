# Freedesktop thumbnailer loader

The `gnome-thumbnailer` loader invokes external thumbnailer commands described by desktop `.thumbnailer` files, then decodes the returned PNG with the builtin loader. It is explicitly selected rather than automatically appended to the normal loader chain. Despite its name, its behavior is driven by installed thumbnailer definitions and tools, not by a guarantee that a GNOME desktop is running.

```sh
img2sixel -L 'gnome-thumbnailer!' document.pdf
```

## Discovery and execution

The current implementation searches `HOME/.local/share/thumbnailers` and the `thumbnailers` subdirectories of `XDG_DATA_DIRS`; if that variable is empty, the system directories default to `/usr/local/share:/usr/share`. This implementation detail should not be expanded into a claim that every XDG location or precedence rule is implemented.

Entries supply MIME matching, `Exec`, and optional `TryExec`. Missing `TryExec` programs cause an entry to be skipped. Placeholder expansion includes the input path/URI, output path, requested size, MIME type, and a literal percent. The adapter runs the expanded command through its process launcher and expects a PNG result. It can also try its fallback thumbnailer command when applicable.

The definition files and helper programs are executable configuration. The backend runs external code and uses temporary output; selecting it changes the process/I/O boundary of image loading. It is not a generic decoder sandbox. A helper's supported formats, dependencies, and startup cost remain part of the deployment.

## What reaches the encoder

`SIXEL_THUMBNAILER_HINT_SIZE` supplies the requested preview size. It is a request to the helper, not a guarantee of the returned dimensions. Shared crop/resize applies afterward. The preview represents a selected still, not the original document's page sequence or animation timeline.

The returned PNG passes through builtin decoding and its pixel/CMS/alpha normalization. This preserves whatever metadata and precision the helper actually wrote into that PNG; it cannot recover metadata or source detail the thumbnailer omitted. Backend-specific source controls from libjpeg, libtiff, or CoreGraphics do not automatically reach the helper.

Use `-x human:trace_topic=loader` to inspect selection and helper progress. `-L gnome-thumbnailer` allows subsequent loader fallback; `!` fixes the chain when diagnosing missing commands or malformed preview output. A helper failure before frame delivery and a downstream callback error have different fallback consequences under the [manager contract](README.md).

## Implementation and validation

[`loader-gnome-thumbnailer.c`](../../src/loader-gnome-thumbnailer.c) owns discovery, entry parsing, MIME matching, placeholder expansion, process execution, temporary cleanup, and builtin PNG handoff. The [thumbnailer suite](../../tests/loader/gnome-thumbnailer/) uses controlled definitions/helpers to test forced selection, wildcard matching, placeholders, `TryExec`, directory search, and fallback. Those fixtures establish integration mechanics; they do not prove that every installed PDF/video/office thumbnailer renders correctly or finishes within a particular latency.
