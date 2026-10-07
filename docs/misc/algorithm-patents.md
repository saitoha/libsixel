# Patent and License Status of Processing Algorithms

## Scope and status of this document

This document records what the libsixel project has been able to establish about patents and licenses affecting the ideas and code used for color management and color spaces, palette construction, palette lookup, dithering, SIXEL encoding and decoding, and decoder-side dequantization. It is not legal advice, and the project has not commissioned a freedom-to-operate analysis. Patent searches are not exhaustive, absence of a known patent is not proof that none exists, and whether a patent claim reads on an implementation depends on its exact wording and on the facts. Readers who need a legal opinion should obtain one from a qualified professional.

Two different questions are kept apart throughout:

- **Patents** can cover a method regardless of who wrote the code. The relevant question is whether a patent in force claims what libsixel does.
- **Copyright licenses** cover code and other expression, not ideas. Implementing an algorithm described in a paper is unaffected by the license of someone else's implementation, but translating or closely adapting that implementation's code can make libsixel's code a derivative work subject to that license.

The SIXEL format itself is covered by [SIXEL patents and royalties](../sixel-format/patents-and-royalties.md), image formats decoded by loaders by [patent status of loader formats](../loader/patents.md), and patent-term rules by the former.

## Summary

| Area | Patent status as far as known | License status |
| --- | --- | --- |
| Color spaces and CMS | No patent claim identified. | Implemented from published standards and papers; the Oklab reference code is public domain or MIT. |
| Palette construction | No patent claim identified; the methods date from 1957 to 2019 and are published algorithms. | Median cut derives from Netpbm code under a permissive notice that libsixel retains. |
| Palette lookup | No patent claim identified. | Implemented from papers; the VP-tree design was informed by libimagequant but its code is independent. |
| Error-diffusion dithering | The relevant patents found, including Canon's variable-coefficient patents, have expired. | Implemented from published kernels. |
| Positional and blue-noise dithering | The blue-noise mask and void-and-cluster patents have expired. | The blue-noise texture is CC0; `a_dither` is public domain. |
| Temporal dithering (`interframe`, `stbn`) | Active NVIDIA patents concern spatiotemporal mask generation and stratified sample generation; libsixel's sources do not perform the claimed generation steps. See [temporal dithering](#temporal-dithering). | Independent implementation. |
| SIXEL encoding and decoding | See the format document; no patent identified for libsixel's scheduling or extensions. | kmiya's code is permissive and relicensed under MIT; DRCS helpers derive from BSD-3-Clause mlterm code. |
| Decoder dequantization (`k_undither` family) | No patent identified. | **Open question.** The implementation closely follows GPL-3.0-or-later code. See [decoder dequantization](#decoder-dequantization). |

## Color spaces and color management

libsixel's color spaces are listed in [Color spaces](../concepts/colorspace/README.md#spaces-used-by-libsixel).

- **sRGB, linear sRGB, SMPTE-C, and Display P3.** These are defined by standards: IEC 61966-2-1 for sRGB, ITU-R BT.709 for its primaries, SMPTE RP 145 for SMPTE-C, and SMPTE EG 432-1 together with the sRGB transfer function for Display P3. The standards documents are copyrighted publications, but implementing their numeric definitions requires no license, and no patent licensing program for them is known.
- **CIELAB and CIEDE2000.** CIE colorimetry, used for working-space coordinates and for the CIEDE2000 metric in the assessment tools. No patent claim is known.
- **DIN99d.** Published by Cui, Luo, Rigg, Roesler, and Witt (2002) and related to the DIN 6176 color-difference standard. A Google Patents full-text search for "din99" in October 2026 returned only patents that use the space within color-matching applications, not patents claiming the transform.
- **Oklab.** Published by Björn Ottosson in 2020. Its [reference code](https://bottosson.github.io/posts/oklab/) is dedicated to the public domain and alternatively offered under the MIT license. No patent claim is known.
- **HLS.** Part of the SIXEL color-register syntax itself; see the format documents.
- **ICC profiles.** The [builtin CMS](../loader/builtin-cms.md) implements a subset of the ICC profile format (ICC.1, also ISO 15076-1) from the specification: matrix/TRC, parametric and sampled curves, and LUT-based transforms. It does not implement black-point compensation or iccMAX. No patent licensing program for implementing ICC.1 is known. The optional lcms2 engine is MIT-licensed, and ColorSync is a platform service.

The project does not use ICtCp, CIECAM02, CAM16, or other appearance models in these paths.

## Palette construction

| Method | Source | Notes |
| --- | --- | --- |
| Median cut (`heckbert`) | Heckbert, 1982 | Derived from Netpbm's `pnmcolormap.c`, itself derived from Jef Poskanzer's `ppmquant`. The permissive Poskanzer and Henderson notice is retained in [`palette-heckbert.c`](../../src/palette-heckbert.c) and in [`LICENSE`](../../LICENSE). |
| k-means and Lloyd refinement | Lloyd, 1957/1982; Hamerly's bound, 2010 | Published algorithms; no patent identified. |
| k-center | Gonzalez, 1985 | Farthest-point clustering; no patent identified. |
| k-medoids | Schubert and Rousseeuw's FasterPAM, 2019 | Implemented from the paper; no patent identified. |
| Final merge | Ward, 1963 | Hierarchical merging; no patent identified. |
| Histogram binning, cover, and snap policies | libsixel | Project-specific policies built from the above. |

These methods are textbook or long-published algorithms. The [quantization guide](../encoder/palette-construction/quantization/README.md) and [merge policy](../encoder/palette-construction/merge-policy/README.md) cite the papers.

## Palette lookup

| Policy | Source | Notes |
| --- | --- | --- |
| `none`, `5bit`, `6bit` | Exhaustive search and direct bucket tables | Common techniques; no patent identified. The `5bit` lineage and comparisons with other SIXEL encoders are described in the [lookup guide](../encoder/palette-application/lookup-policy/README.md). |
| `certlut` | k-d tree (Bentley, 1975) with libsixel's certification rule | No patent identified. |
| `eytzinger` | Khuong and Morin, 2015 | Array layout for comparison search; no patent identified. |
| `fhedt` | Felzenszwalb and Huttenlocher's distance transform | No patent claiming the transform was identified; full-text hits only use it in other applications. |
| `vptree` | Uhlmann, 1991; Yianilos, 1993 | No patent identified. The previous-index acceleration was informed by libimagequant (GPL-3.0-or-later), which is an idea-level influence: libsixel's tree is built differently (it uses the last index as the vantage point rather than libimagequant's popularity-ranked choice) and shares no code structure with libimagequant's `nearest.c`. |
| `rbc` | Cayton's random ball cover, 2012 | No patent identified. |
| `mahalanobis` | Mahalanobis, 1936 | No patent identified. |

GPU palette application on Metal uses Apple's platform API and introduces no separate patent question known to the project.

## Error-diffusion dithering

Floyd–Steinberg (1976), Jarvis–Judice–Ninke (1976), Stucki (1981), Burkes (1988), the Sierra kernels (1989–1990), and Atkinson's partial diffusion are published kernels from 1976 to 1990. No patent on them is known, and any patent from that period would have expired.

`lso2` is libsixel's own variable-coefficient policy, influenced by Ostromoukhov's *A Simple and Efficient Error-Diffusion Algorithm* (2001) and Zhou and Fang's threshold modulation (2003). Ostromoukhov's variable-coefficient error diffusion was patented by Canon: [U.S. Patent 5,737,453](https://patents.google.com/patent/US5737453A/en), filed on 17 May 1996, and [U.S. Patent 6,356,362](https://patents.google.com/patent/US6356362B1/en), filed on 2 November 1998. Both have expired. No patent for Zhou and Fang's method was found. `lso2` also differs technically: it selects coefficients by the magnitude of the quantization error rather than by input intensity, and its coefficients and six-position stencil are libsixel-specific.

The paired coefficient perturbation of the `fs` policy is libsixel's own; random perturbation of diffusion weights was already discussed in Ulichney's *Digital Halftoning* (1987).

## Positional and blue-noise dithering

- **`a_dither` and `x_dither`.** Øyvind Kolås describes [a dither](https://pippin.gimp.org/a_dither/) as public domain. No patent claim is known.
- **`bluenoise`.** The embedded 64-by-64 texture is Christoph Peters's blue-noise texture from Moments in Graphics, released under CC0 1.0; see [`bluenoise_64x64.h`](../../src/bluenoise_64x64.h) and [`LICENSE`](../../LICENSE). Halftoning with a blue-noise mask was patented by Research Corporation Technologies in [U.S. Patent 5,111,310](https://patents.google.com/patent/US5111310A/en), filed on 4 December 1990, and DEC patented Ulichney's void-and-cluster generation in [U.S. Patent 5,535,020](https://patents.google.com/patent/US5535020A/en), filed on 15 October 1992, with continuations filed in 1996 and 1997. All of these have expired.

## Temporal dithering

The `interframe` policy carries quantization residuals from one animation frame to the next. Temporal error feedback for displays and video has a long patent history, and the project has not cleared that area exhaustively. No patent in force claiming libsixel's residual feedback has been identified; a 2004 Kodak application on video tone-scale reduction found in searching has passed the 20-year term in any case.

The `stbn` policy adds a frame- and position-dependent perturbation from one of three sources. The name refers to the objective of spatiotemporal blue noise, which NVIDIA researchers described in *Spatiotemporal Blue Noise Masks* (2022). NVIDIA holds active patents in this area:

- [U.S. Patent 11,727,621](https://patents.google.com/patent/US11727621B2/en) (priority 16 December 2021, anticipated expiry 2041) claims generating a texture by computing an energy function over pixel pairs within layers or across temporal slices and swapping pixels until the energy is minimized.
- [U.S. Patent 12,283,028](https://patents.google.com/patent/US12283028B2/en) (priority 22 November 2021, adjusted expiry 2043) claims computing such energy values, generating a mask from them, and applying the mask over multiple frames.
- [U.S. Patent 11,941,743](https://patents.google.com/patent/US11941743B2/en) (priority 14 December 2018, anticipated expiry 2039) claims generating sample points by tracking valid regions within elementary intervals of a stratified two-dimensional space.

libsixel's `stbn` sources do not compute energy functions, optimize masks, or track valid regions within elementary intervals. The `mask` source derives temporal variation from the fixed CC0 two-dimensional texture with integer hashing. The `pmj` source, named after Christensen, Kensler, and Kilpatrick's *Progressive Multi-Jittered Sample Sequences* (Pixar, 2018), forms a Morton code over a 64-by-64 tile and applies hash-based permutations; no Pixar patent on that paper was found. The `hash` source uses Chris Wellons's public-domain lowbias32 integer hash. These differences are recorded as facts about the implementation, not as a legal conclusion that the patents do not apply. Changes that would generate optimized spatiotemporal masks, embed masks produced by such a process, or implement stratified sample generation should be reviewed against these patents first.

## SIXEL encoding and decoding

- **Body encoding.** The palette-plane encoder derives from kmiya's `sixel` (2014), whose author distributed it under a permission to use, modify, and redistribute without restriction; libsixel relicensed it under MIT, as recorded in [`encoder-core-encode.c`](../../src/encoder-core-encode.c) and [`LICENSE`](../../LICENSE). Node scheduling, `-E size`, the OR-mode dialect, and delta encoding are libsixel's own. No patent on them is known.
- **High-color mode.** Contributed by Araki Ken to libsixel; see the [project history](../project-history.md).
- **DRCS output.** Portions of [`drcs.c`](../../src/drcs.c) derive from mlterm's `drcssixel.c` under the BSD-3-Clause license, whose notice is retained in the source and in [`LICENSE`](../../LICENSE).
- **Decoding.** The SIXEL parser in [`fromsixel.c`](../../src/fromsixel.c) derives from kmiya's code under the same permission. Its `hls2rgb` helper is imported from Ross Combs's `graphics.c` in xterm under that file's permissive notice, as recorded in [`LICENSE`](../../LICENSE).

## Decoder dequantization

`sixel2png` and the decoder API offer dequantization methods that estimate colors a palette could not represent. The `k_undither` family is based on Kornel Lesiński's [undither](https://github.com/kornelski/undither), as the [dequantization guide](../decoder/dequantization/README.md) explains. No patent on the method is known.

undither is licensed under GPL-3.0-or-later. Using its idea is unaffected by that license, but libsixel's implementation in [`decoder.c`](../../src/decoder.c) follows the structure of undither's code closely: a palette-pair similarity cache with the same sentinel and diagonal values, the same midpoint construction and nearest-other-color test, and a Prewitt-gradient gate with the same thresholds, 160 and 256 at the default edge strength, although libsixel's weights and options differ. Whether this makes the code a derivative work of undither, and therefore incompatible with libsixel's MIT license, is unresolved. Until it is resolved by the project, this code should not be described as independently written. Possible resolutions are permission from the author to use the adapted code under MIT, a reimplementation from a written description by a developer who has not read the undither code, or removal of the affected methods.

## Third-party notices

[`LICENSE`](../../LICENSE) collects the notices for code and data from other projects, including kmiya's `sixel`, stb, the Netpbm-derived median cut, mlterm's DRCS helper, the `hls2rgb` helper from xterm's `graphics.c`, the CC0 blue-noise texture, test images, and build-system helpers. A change that adapts code from another project must add its notice there and in the affected source file in the same change, and must not adapt code whose license is incompatible with distribution under the MIT license.

## Maintenance

When a new color, palette, lookup, dithering, encoding, or decoding method is added, record its source, any patent identified, and whether code or only ideas were taken from another implementation. Evidence that changes any statement above should be recorded with its source.

## References

- Victor Ostromoukhov, [*A Simple and Efficient Error-Diffusion Algorithm*](https://doi.org/10.1145/383259.383326), SIGGRAPH 2001; Canon's [U.S. Patent 5,737,453](https://patents.google.com/patent/US5737453A/en) and [U.S. Patent 6,356,362](https://patents.google.com/patent/US6356362B1/en).
- Kevin J. Parker and Theophano Mitsa, [U.S. Patent 5,111,310](https://patents.google.com/patent/US5111310A/en); Robert Ulichney, [U.S. Patent 5,535,020](https://patents.google.com/patent/US5535020A/en).
- Alan Wolfe et al., [*Spatiotemporal Blue Noise Masks*](https://research.nvidia.com/publication/2022-07_spatiotemporal-blue-noise-masks), 2022; NVIDIA's [U.S. Patent 11,727,621](https://patents.google.com/patent/US11727621B2/en), [U.S. Patent 12,283,028](https://patents.google.com/patent/US12283028B2/en), and [U.S. Patent 11,941,743](https://patents.google.com/patent/US11941743B2/en).
- Per Christensen, Andrew Kensler, and Charlie Kilpatrick, [*Progressive Multi-Jittered Sample Sequences*](https://graphics.pixar.com/library/ProgressiveMultiJitteredSampling/), 2018.
- Björn Ottosson, [*A perceptual color space for image processing*](https://bottosson.github.io/posts/oklab/), 2020.
- Øyvind Kolås, [*a dither*](https://pippin.gimp.org/a_dither/).
- Christoph Peters, [*Free blue noise textures*](https://momentsingraphics.de/BlueNoise.html).
- Kornel Lesiński, [undither](https://github.com/kornelski/undither), GPL-3.0-or-later; [libimagequant](https://github.com/ImageOptim/libimagequant), GPL-3.0-or-later.
