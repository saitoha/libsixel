# Transparent Pixel Policy

The transparent pixel policy defines where source alpha is resolved and what
an omitted SIXEL cell means. It is selected with
`img2sixel --transparent-policy=POLICY` (short form `-A POLICY`) or the
`SIXEL_TRANSPARENT_POLICY` environment variable. An explicit command-line
option takes precedence over the environment.

## Policies

| Policy | Loader treatment | SIXEL treatment |
| --- | --- | --- |
| `background` | Composite source alpha over the resolved background. | Emit opaque output when composition succeeds. If no background can be resolved, retain alpha-zero pixels and use the `P2=1` fallback. |
| `transparent` | Preserve alpha-zero pixels. Semi-transparent pixels are composited when a background is available because SIXEL has no partial-alpha representation. | Omit alpha-zero pixels and emit DCS `P2=1`. |
| `clear` | Use the same alpha-zero preservation as `transparent`. | Omit alpha-zero pixels and emit DCS `P2=0`. |

`background` is the default. `composite` is its compatibility alias. `keep`,
`keep-destination`, `previous`, `p2-1`, and `p21` are aliases for
`transparent`. `p2-0` and `p20` are aliases for `clear`.

`P2=1` asks a terminal to leave the existing image-plane pixel unchanged when
the SIXEL stream does not paint that cell. `P2=0` asks it to clear omitted
cells to the terminal background. These wire semantics do not perform source
alpha blending; blending belongs to the loader.

## Background resolution

The loader may resolve a background from these sources:

- `-B COLOR` or `SIXEL_BGCOLOR`;
- an OSC 11 terminal background reply when probing is enabled and succeeds;
- a file background such as PNG `bKGD` or the GIF logical-screen background.

`SIXEL_BACKGROUND_POLICY` selects whether a supported file background or an
explicit color wins. OSC 11 colors are terminal UI colors and are interpreted
as gamma-encoded values. See `img2sixel -H` for the background source,
priority, colorspace, and OSC 11 controls.

When `background` cannot resolve any source, the loader must not expose hidden
RGB from fully transparent pixels as visible output. It retains the
alpha-zero mask, and the encoder emits `P2=1` as the safe compatibility
fallback.

## Loader and encoder boundary

The loader owns alpha normalization. If it composites onto a background, the
resulting frame is opaque and must not carry an alpha-zero mask. If it
preserves alpha zero, it records that state in the frame's transparent index
or per-pixel mask.

The encoder trusts that frame state. It reserves a key color only when the
frame still contains transparency, then selects `P2` from the requested
policy. It must not discard a loader-provided mask merely because the policy
is named `background`: such a mask means that background resolution failed
and the loader intentionally used the transparent fallback.

The builtin RGBA and non-indexed PNG paths preserve source RGB for alpha-zero
pixels under `transparent` and `clear`, while still compositing partial alpha
over an available background. Under `background`, they composite all alpha,
including alpha zero, and clear the mask after successful composition.

## Related options

`--transparent-offset` requires `transparent` semantics because positional
padding relies on `P2=1` image-plane reuse. `clear` and `background` are
rejected when an explicit transparent offset is active. 6delta accumulation
uses the same `P2=1` contract.

Regression coverage is split between loader tests, which verify pixel
composition and mask retention, and encoder/palette tests, which verify DCS
header selection and the default policy.
