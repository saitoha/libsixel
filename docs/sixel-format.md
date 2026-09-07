# SIXEL Format

## Scope

SIXEL is a paletted bitmap representation carried inside a terminal Device
Control String (DCS). Digital Equipment Corporation introduced it for printers
and later used it for graphics terminals including the VT240, VT330, and
VT340.

This document is a practical format guide for libsixel users and contributors.
It describes the common DEC terminal form and the conventions that matter to
this implementation. It is not a substitute for the device manual: historical
printers, DEC terminals, and modern terminal emulators differ in limits,
palette lifetime, aspect-ratio handling, scrolling, and error recovery.

## DCS envelope

In a 7-bit environment, a complete image has this shape:

```text
ESC P [P1 [; P2 [; P3]]] q SIXEL-DATA ESC \
```

The byte sequences are:

- `ESC P` (`0x1b 0x50`): 7-bit DCS introducer;
- `q` (`0x71`): SIXEL protocol selector;
- `ESC \` (`0x1b 0x5c`): 7-bit String Terminator (ST).

An 8-bit C1 environment can encode DCS as `0x90` and ST as `0x9c`. The body is
otherwise the same. The 7-bit form is generally safer across UTF-8 transports,
terminal multiplexers, loggers, and tools that do not preserve raw C1 bytes.

The spaces and brackets above are notation only; they are not transmitted.
For example, libsixel commonly begins a 7-bit image with `ESC P 0;0;0 q`
without spaces.

### Header parameters

`P1`, `P2`, and `P3` are decimal parameters before `q`.

`P1` is the legacy macro parameter for pixel aspect ratio:

| `P1` | Vertical:horizontal pixel aspect ratio |
| --- | --- |
| omitted, `0`, or `1` | `2:1` |
| `2` | `5:1` |
| `3` or `4` | `3:1` |
| `5` or `6` | `2:1` |
| `7`, `8`, or `9` | `1:1` |

DEC recommended that new applications use `P1=0` and specify pixel geometry
with the raster-attributes command instead.

`P2` selects background handling. With `0`, `2`, or an omitted value,
unpainted positions use the device's current background color. With `1`, those
positions retain their previous contents. For a decoder that produces an image
rather than drawing on an existing terminal surface, the second behavior
requires an explicit transparency or paint-mask policy.

`P2` does not add an alpha channel to SIXEL. See
[Pixel Formats and Alpha Representation](concepts/pixelformat.md) for the
distinction between input alpha, frame transparency metadata, and omitted
SIXEL pixels.

`P3` is the horizontal grid-size parameter. The VT300 ignored it because its
grid size was fixed. Modern applications should not rely on it for output
dimensions.

## Sixel data characters

A sixel is one vertical column of six pixels. Data bytes range from `?`
(`0x3f`) through `~` (`0x7e`). Subtract `0x3f` from the byte to obtain a six-bit
mask:

```text
mask = byte - 0x3f
```

Bit 0 is the top pixel and bit 5 is the bottom pixel. A set bit paints the
currently selected color. After one data character, the active position moves
one column to the right.

| Character | Mask | Effect |
| --- | --- | --- |
| `?` | `000000` | Paint no pixel in this column |
| `@` | `000001` | Paint the top pixel |
| `A` | `000010` | Paint the second pixel from the top |
| `~` | `111111` | Paint all six pixels |

Six-pixel-high bands and horizontal advancement are the essential layout
model. There is no separate width-and-height footer. `Ph` and `Pv` are the only
explicit extents; without them, a raster decoder must infer bounds from active
positions and painted pixels. Trailing blank columns or bands can therefore be
ambiguous or cropped by an implementation. Emit raster attributes when exact
canvas dimensions matter.

## Body control functions

### Graphics Repeat Introducer: `!`

```text
! Pn sixel-character
```

`Pn` is a decimal repeat count. The following data character is processed
`Pn` times. For example, `!20~` represents twenty fully painted columns.

Repeat counts are untrusted input. Implementations must detect numeric overflow
and apply documented image-size and resource limits before expanding a run.
Some historical devices limit the repeat argument to 255; libsixel exposes a
compatibility option for that restriction.

### Set Raster Attributes: `"`

```text
" Pan ; Pad ; Ph ; Pv
```

`Pan` and `Pad` are the numerator and denominator of the vertical:horizontal
pixel aspect ratio. `Ph` and `Pv` declare the horizontal and vertical image
extents in pixels. The command belongs before sixel image data and overrides
the legacy `P1` aspect-ratio selection.

The declared extents do not clip later sixel data. DEC devices used them to
establish the background area and to encode dimensions without transmitting
blank columns. A decoder must therefore account for both declared extents and
the actual positions reached by the body, while rejecting values that exceed
its resource limits.

### Graphics Color Introducer: `#`

Select an existing color register with:

```text
# Pc
```

Define and select a color register with:

```text
# Pc ; Pu ; Px ; Py ; Pz
```

- `Pc` is the color-register number.
- `Pu=1` selects HLS coordinates: hue is `0..360`, while lightness and
  saturation are `0..100` percent.
- `Pu=2` selects RGB coordinates: red, green, and blue are each `0..100`
  percent.

DEC defined `Pc` as a color-register number in the range `0..255`, providing a
namespace of at most 256 registers. The VT330 implemented only four registers
and the VT340 only sixteen. Register numbers above 255 are
implementation-specific extensions. Applications must not confuse the DEC
numbering range with a device's usable palette capacity.

The 256-register namespace does not by itself limit a complete stream to 256
distinct RGB definitions. A stream can redefine and reuse a register after
painting has begun, as libsixel's high-color mode does. Whether earlier pixels
retain their painted RGB values or change with the live register is a separate,
device-specific rendering behavior.

Color registers are state. Palette lifetime and whether a DCS receives private
or shared registers vary among terminals. Portable output should define every
register it depends on and should not assume that an earlier image established
the current palette.

### Graphics Carriage Return: `$`

`$` returns the active horizontal position to the graphics left margin without
advancing to the next sixel band. Encoders use it to overprint the same band
with another color plane.

### Graphics New Line: `-`

`-` returns to the graphics left margin and advances to the next sixel band.
For a 1:1 raster this advances the logical vertical position by six pixels.

## Rendering model

SIXEL is not a stream of packed RGB pixels. It is a sequence of masks painted
with selected palette registers. A typical encoder performs these operations:

1. define the registers used by the image;
2. select one register;
3. emit mask characters for pixels of that color in a six-pixel-high band;
4. use `$` to return to the band's left edge and paint another color plane;
5. use `-` to advance after the band is complete;
6. terminate the DCS with ST.

The format does not require one particular ordering of color planes. kmiya's
encoder and libsixel optimize ordering and repeated runs to reduce transmitted
data while preserving the same rendered result.

### Minimal example

The notation below uses `<ESC>` to make escape bytes visible:

```text
<ESC>P0;1;0q"1;1;3;6#1;2;100;0;0@A?<ESC>\
```

It declares a 3 by 6 raster, defines register 1 as red, and sends masks `@`,
`A`, and `?`. Ignoring the existing background, the painted positions are:

```text
#..
.#.
...
...
...
...
```

`#` in this diagram means a painted pixel; it is not another color command.

## Terminal placement and scrolling

SIXEL rendering begins relative to a terminal or graphics active position, not
an intrinsic page origin shared by every implementation. DEC's Sixel Display
Mode (DECSDM, private mode 80) controls whether the sixel active position can
scroll and whether leaving sixel mode updates the text cursor. Modern terminals
have additional placement and scrolling policies.

Applications that need predictable inline placement must treat terminal
capability and cursor policy as a separate contract from the SIXEL bitmap body.
Do not infer placement behavior only from successful image decoding.

## Images, files, and animation

A `.six` file normally contains a terminal control stream rather than a
container with an independent magic header, directory, or checksum. It may
contain one DCS image, several images, or surrounding terminal controls.

Animation is not a distinct DEC SIXEL file format. It is produced by sending a
series of SIXEL images and coordinating palette reuse, placement, erasure, and
timing outside the individual bitmap body. libsixel can generate such streams,
but a decoder of one DCS must not silently invent animation semantics.

Likewise, libsixel's high-color mode is an encoding technique built from normal
SIXEL operations, including repeated palette definitions and painting passes;
it is not a new sixel data-character range.

## Robustness and security

SIXEL is both image data and executable terminal control syntax. Do not send an
untrusted `.six` file directly to a terminal merely because the suffix suggests
an image.

Encoders and decoders must preserve these boundaries:

- terminate every DCS after its introducer has been emitted, including error
  paths where termination remains possible;
- never interleave diagnostics, shell text, OSC, or unrelated control strings
  inside an open SIXEL DCS;
- bound numeric parameters before multiplication, allocation, or run
  expansion;
- treat declared dimensions as claims, not trusted allocation sizes;
- keep palette indices separate from transparent or unpainted sentinels;
- reject or safely recover from truncated DCS, invalid color definitions, and
  body positions beyond configured limits;
- preserve exact ownership and partial-output rules in library APIs.

An unterminated DCS can make a terminal appear hung because later shell output
is still being consumed as control-string data. This is a protocol-state
failure even when the encoder already reported its original error.

## libsixel implementation map

- [`src/sixel-writer.c`](../src/sixel-writer.c) owns DCS and header emission,
  transport wrapping, and string termination.
- [`src/encoder-core-encode.c`](../src/encoder-core-encode.c) contains the core
  palette-plane and sixel body encoding logic developed from kmiya's approach,
  including recovery after a late encoding error.
- [`src/fromsixel.c`](../src/fromsixel.c) parses DCS parameters, raster
  attributes, repeat commands, color commands, and sixel masks.
- [`src/output.c`](../src/output.c) owns output configuration such as omitting
  the DCS envelope for integrations that transport a body separately.
- [`include/sixel.h.in`](../include/sixel.h.in) defines the public encoder,
  decoder, palette, and compatibility contracts.
- [`src/sixel.5`](../src/sixel.5) is the installed manual-page overview.

## Primary references

- Digital Equipment Corporation,
  [*VT330/VT340 Programmer Reference Manual, Volume 2: Graphics Programming*,
  second edition, May 1988](https://bitsavers.trailing-edge.com/pdf/dec/terminal/vt340/EK-VT3XX-GP-002_VT330_VT340_Graphics_Programming_198805.pdf),
  especially Chapter 14.
- [HTML transcription of Chapter 14, Sixel Graphics](https://manx-docs.org/mirror/vt100.net/docs/vt3xx-gp/chapter14.html).
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html),
  for the behavior and extensions of a widely deployed modern implementation.

Where these references and a terminal emulator disagree, document the
interoperability decision and cover the intended behavior with a focused test.
