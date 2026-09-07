# Alpha Policy

The alpha policy defines where source alpha is resolved and what an omitted
SIXEL pixel asks the terminal to do. It is selected with
`img2sixel --alpha-policy=POLICY` (short form `-A POLICY`) or the
`SIXEL_ALPHA_POLICY` environment variable. An explicit command-line option
takes precedence over the environment.

## Policies

| Policy | Loader treatment | SIXEL treatment |
| --- | --- | --- |
| `auto` | Preserve source alpha without terminal background probing. | Select `clear` normally and `keep` when transparent offset or 6delta output requires retained pixels. |
| `composite` | Composite source alpha over the resolved background. | Emit opaque output when composition succeeds. If no background can be resolved, fall back to `keep`. |
| `clear` | Preserve alpha-zero pixels. | Omit alpha-zero pixels and emit DCS `P2=0`, requesting the terminal to clear omitted pixels. |
| `keep` | Preserve alpha-zero pixels. | Omit alpha-zero pixels and emit DCS `P2=1`, requesting the terminal to keep existing pixels. |

`auto` is the default. These are the only accepted policy names.
Semi-transparent pixels are composited when a background is available because
SIXEL cannot represent partial alpha.

## Visual comparison

The following APNG illustrates the policies with a black terminal background
and a translucent window. `composite` produces an opaque image rectangle,
`clear` removes omitted pixels on each frame, and `keep` leaves earlier image
pixels in place. This is an illustration of the requested operations, not a
promise that every terminal will produce the same pixels.

![Animated comparison of composite, clear, and keep](assets/alpha-policy-comparison.png)

A [GIF fallback](assets/alpha-policy-comparison.gif) is available for viewers
without APNG animation support. The bouncing-ball source is by Holger Will and
is [released into the public domain](https://commons.wikimedia.org/wiki/File:Animated_PNG_example_bouncing_beach_ball.png).

`clear` and `keep` name the requested operation rather than claiming portable
transparency. SIXEL renderers differ in how graphics interact with the text
plane, earlier graphics, and a translucent terminal window. In xterm and
mlterm, `P2=1` holes commonly keep the existing graphics content. Other
terminals can produce a different visible result: P2 rendering is
terminal-dependent. `P2=0` and `P2=1` never perform source alpha blending;
blending belongs to the loader.

## Background resolution

The loader may resolve a background from these sources:

- `-B COLOR` or `SIXEL_BGCOLOR`;
- an OSC 11 terminal background reply when probing is enabled and succeeds;
- a file background such as PNG `bKGD` or the GIF logical-screen background.

`--background-policy=POLICY` selects whether a supported file background or an
external color wins. `file_first` is the default; `explicit_first` prefers
`-B`, `SIXEL_BGCOLOR`, or a successful OSC 11 reply. The dedicated option
overrides the loader suboption and `SIXEL_BACKGROUND_POLICY`. OSC 11 colors
are terminal UI colors and are interpreted as gamma-encoded values. OSC 11 is
queried only for effective `composite` policy. See `img2sixel -H` for the
background source, priority, colorspace, and OSC 11 controls.

When `composite` cannot resolve any source, the loader must not expose hidden
RGB from fully transparent pixels as visible output. It retains the alpha-zero
mask, and the encoder falls back to the `keep` request with `P2=1`.

## Loader and encoder boundary

The loader owns alpha normalization. If it composites onto a background, the
resulting frame is opaque and must not carry an alpha-zero mask. If it
preserves alpha zero, it records that state in the frame's transparent index
or per-pixel mask.

The encoder trusts that frame state. It reserves a key color only when the
frame still contains transparency, then selects `P2` from the requested
policy. It must not discard a loader-provided mask merely because the policy
is `composite`: such a mask means that background resolution failed and the
loader intentionally used the `keep` fallback.

The builtin RGBA and non-indexed PNG paths preserve source RGB for alpha-zero
pixels under `clear` and `keep`, while still compositing partial alpha over an
available background. Under `composite`, they composite all alpha,
including alpha zero, and clear the mask after successful composition.

## Related options

`--transparent-offset` and 6delta output require `keep` because their retained
pixels rely on the `P2=1` image-plane request. `auto` selects `keep` for either
feature. Explicit `clear` and `composite` are rejected when either feature is
active.

Regression coverage is split between loader tests, which verify pixel
composition and mask retention, and encoder/palette tests, which verify DCS
header selection and the default policy.
