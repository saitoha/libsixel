# Builtin Softimage PIC Loader

## Identity and history

Softimage PIC remains in the adapted stb-derived core imported with commit [`81375f0bd`](https://github.com/saitoha/libsixel/commit/81375f0bd). Recognition requires the four-byte magic `53 80 f6 34` and `PICT` at byte offset 88. libsixel's routing requests an RGBA intermediate and applies the common alpha policy; dedicated regression coverage was added in commit [`813c8e08e`](https://github.com/saitoha/libsixel/commit/813c8e08e) in 2026.

## Accepted variants

Width and height are read as 16-bit big-endian values. The decoder accepts up to ten chained channel packets. Every packet must contain 8-bit samples and may select any combination of red, green, blue, and alpha channels. Compression type 0 is uncompressed, type 1 is pure run-length encoding, and type 2 is mixed raw/run-length encoding. Packet data for the same channel may appear in more than one packet.

The decoder initializes a four-component RGBA canvas, decodes packet-selected components, reports RGB or RGBA according to the channel mask, and then converts to the requested RGBA intermediate. The builtin frame path applies background or retained-transparency policy and otherwise treats color samples as gamma eight-bit RGB.

## Not supported or not interpreted

Packet sample sizes other than 8 bits, compression types other than 0–2, more than ten packets, and malformed scanline runs are rejected. The header's pixel ratio, fields, and padding values are skipped; they do not alter geometry or interlace behavior. There is no embedded ICC interpretation, orientation metadata, high-depth output, animation, or general preservation of unrecognized header data.
