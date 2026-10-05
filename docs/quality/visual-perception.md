# Visual Perception for Image Quality

## Scope

This document explains the sensory physiology and perception that libsixel's image-quality evaluation rests on: what perceptual quality means, how the eye encodes color, how it adapts to light, how its sensitivity depends on spatial frequency, how viewing geometry maps pixels onto the retina, and why fine noise and fine chroma detail are less visible than coarse error and luminance structure.

[Image-quality metrics](metrics.md) defines the metrics `lsqa` reports and explains how they apply this background. [Color difference and color reproduction](color-difference.md) applies it to color differences, including dark colors and viewing situations. The [image-quality measurement policy](measurement-policy.md) owns thresholds and comparison conditions.

## Perceptual quality

Image *fidelity* is how closely a reproduction matches a reference. Image *quality* is how good an image is judged to be. They can diverge: a sharpened reproduction can be preferred to an exact one. The metrics in [image-quality metrics](metrics.md) are *full-reference* fidelity metrics: they need the reference and estimate how visible or objectionable the departure from it is.

The ground truth for any such metric is human judgment collected under controlled conditions, for example by the methods in ITU-R Recommendation BT.500 for television pictures, summarized as mean opinion scores. A metric is "perceptual" to the extent that its predictions agree with those judgments across many images and distortion types. SSIM, MS-SSIM, GMSD, and CIEDE2000 were each validated against such data; the libsixel-specific descriptors were not.

libsixel aims at the fidelity of the image the viewer perceives, not of the stored pixels: within the limits of a palette image, the output should look like the reference. Pixel values may differ widely, as they do in any dithered image, as long as the perceived image does not. What a viewer perceives depends on conditions that a file does not record: the light level the viewer is adapted to, and the viewing distance and pixel size that set spatial frequency on the retina. The sections below explain how these conditions shape perception.

## Photoreceptors and opponent channels

Daylight vision begins with three cone classes, sensitive to long, medium, and short wavelengths (L, M, S). The retina and lateral geniculate nucleus recombine the cone signals into an achromatic channel, roughly L + M, and two opponent chromatic channels, roughly L − M (red-green) and S − (L + M) (blue-yellow). This recoding was proposed in the opponent-process theory of Hering and quantified by Hurvich and Jameson (1957). S cones are sparse and absent from the center of the fovea, which already limits blue-yellow spatial resolution.

CIELAB, the opponent coordinates OKLab and DIN99d, and YCbCr video all reflect this organization: one lightness axis and two opponent chroma axes. The achromatic channel is the one with high spatial resolution.

## Light level and adaptation

The visual system works over roughly ten orders of magnitude of luminance, from starlight to sunlit snow, but at any moment it uses a far narrower operating range set by *adaptation*. Two photoreceptor systems divide that span.

**Cones and rods.** The human retina has about six million cones and more than ninety million rods. Cones come in the three classes described above, so they support color vision; they are concentrated in the fovea and need comparatively high light levels. Rods contain a single photopigment, rhodopsin, so they cannot distinguish wavelength and give no color by themselves; they are far more sensitive, are absent from the central fovea (about one degree across), and dominate peripheral vision. In bright light rods are saturated and contribute little (Aguilar and Stiles, "Saturation of the rod mechanism of the retina at high levels of stimulation", Optica Acta 1(1), 1954).

**Photopic, mesopic, and scotopic vision.** Which system carries vision depends on the adaptation luminance:

| Regime | Approximate luminance | Receptors | Consequences |
| --- | --- | --- | --- |
| Photopic | above about 5 cd/m² | cones | full color vision, highest acuity, spectral sensitivity `V(λ)` peaking near 555 nm |
| Mesopic | about 0.005 to 5 cd/m² | cones and rods together | color discrimination and acuity decline progressively; blue-green looks relatively brighter and red darker |
| Scotopic | below about 0.005 cd/m² | rods | no hue discrimination, low acuity, the fovea is nearly blind, spectral sensitivity `V'(λ)` peaking near 507 nm |

The mesopic limits are those of CIE 191:2010, which defines photometry for that range. The movement of peak sensitivity from about 555 nm to about 507 nm as light falls is the Purkinje shift: in dusk, red flowers darken while blue-green foliage remains visible. Colorimetry, including CIELAB and CIEDE2000, assumes photopic vision throughout.

**How adaptation works.** Several mechanisms adjust sensitivity to the prevailing light:

- the pupil changes diameter between about 2 and 8 mm, a factor of about 16 in light admitted, which covers only a small part of the full range;
- photopigment bleaching and regeneration reduce receptor sensitivity in bright light and restore it in darkness;
- neural gain control in the retina scales each receptor class's signal to the local mean, so that the response encodes *contrast*, the ratio to the adapted level, rather than absolute light.

The last mechanism is why vision follows Weber's law over much of the photopic range: the just-visible luminance step is a roughly constant fraction of the adapted luminance. At lower light levels photon noise dominates and the threshold grows more slowly than the luminance (the de Vries–Rose regime), so relative sensitivity falls. CIELAB's cube-root lightness and SSIM's ratio-based luminance term both encode the Weber-law idea.

**Time course.** Light adaptation, from dark to bright, takes from seconds to a minute or two. Dark adaptation is slow. Cone thresholds reach a plateau after about ten minutes in darkness, at the rod–cone break rods become the more sensitive system, and full rod sensitivity takes thirty to forty minutes. A viewer who has just looked at a bright window is not adapted to a dim screen, and the reverse.

**Chromatic adaptation.** Gain control acts separately on the three cone classes, so the visual system also adapts to the color of the illumination: a white paper looks white under both daylight and incandescent light. Von Kries ("Chromatic adaptation", 1902) modeled this as independent scaling of each cone response. CIELAB's division of `X`, `Y`, and `Z` by the white point is a simplified version of the same idea, applied to XYZ rather than to cone responses; color appearance models such as CIECAM02 and CAM16 use a proper cone-space adaptation transform and also take the absolute adapting luminance as a parameter.

**Global and local adaptation.** The pupil and the balance between rods and cones follow the light of the whole visual field. Retinal gain control is partly local, and the eyes move constantly between regions, so the adapted state also varies across the scene and over time. A dark region inside a bright image therefore does not switch vision to rods: it is seen by light-adapted cones, whose signal is weak and whose contrast against the surrounding adaptation level is low. Vision becomes mesopic only when the overall field is dim, as with a dark room, a low display brightness, and content that is mostly dark. Image appearance models such as iCAM build local adaptation into their processing for this reason.

**Displays.** An SDR display typically has a white of 80 to 300 cd/m² and a black of a few tenths of a cd/m² or less, before room reflections. With an 80 cd/m² white, CIELAB `L* = 30` corresponds to about 5 cd/m², the photopic boundary, and `L* = 10` to about 0.9 cd/m². Shadows in an image routinely fall below the photopic boundary in absolute terms, but whether the viewer is mesopic depends on the whole field, as described above. [Dark colors and luminance level](color-difference.md#dark-colors-and-luminance-level) explains what this means for color differences.

**Display color filters.** Night-time display modes such as Night Shift and Night Light, and similar warm-tint utilities, lower the correlated color temperature of the display white by reducing its short-wavelength output. Their stated purpose concerns a third class of retinal photoreceptor, the intrinsically photosensitive retinal ganglion cells containing melanopsin (peak sensitivity near 480 nm). These cells feed a non-image-forming pathway that regulates the circadian clock and melatonin secretion; they are separate from the cone and rod adaptation described above, and the health benefit of display tinting is still debated. The filters still interact with vision through chromatic adaptation. After a few minutes the tinted white looks nearly white again, but adaptation to a self-luminous display is incomplete, and the room light competes with it, so a warm cast usually remains; CIECAM02 represents this with a degree of adaptation below one. Adaptation restores only the white reference. Because the filter removes part of the blue primary's output, the display gamut shrinks, and blues and violets that differed before remain harder to tell apart after adaptation. The filter acts on every image equally, so side-by-side comparisons of a reference and an output stay meaningful. Absolute judgments of color, such as whether a gray is neutral or a skin tone natural, are biased under a filter and should be made with it off. `lsqa` reads encoded values and is unaffected by any display filter.

**Dark terminals and dark rooms.** A full-screen terminal with a black background can lower the adaptation level, but only under some conditions. In a lit room, office lighting of a few hundred lux keeps the surrounding surfaces at tens of cd/m², and vision stays photopic. In a dark room, most of the field is the display black, roughly 0.05 to 0.5 cd/m² for an LCD and nearly zero for an OLED, which lies in the mesopic range. Several factors keep adaptation from going deep: text and images at tens to hundreds of cd/m² stay in view and pull adaptation upward, bright areas weigh more heavily than dark ones, reading fixates the rod-free fovea, a 27-inch display at 60 cm spans only about 53 degrees horizontally, many dark themes use a dark gray rather than black, and rod sensitivity needs tens of minutes of continuous darkness while one bright image restores light adaptation within seconds. The practical result of long dark-room work on a dark theme is adaptation near the photopic–mesopic boundary: steps between near-black levels become easier to see, color discrimination in dark areas declines slightly, and a suddenly displayed bright image causes glare. How this changes the visibility of near-black errors is described under [dark colors and luminance level](color-difference.md#dark-colors-and-luminance-level).

## Contrast sensitivity

The contrast sensitivity function (CSF) gives the reciprocal of the smallest visible contrast of a sinusoidal grating as a function of spatial frequency in cycles per degree of visual angle (cpd). Its shape differs by channel:

- **Achromatic:** band-pass. Under photopic conditions sensitivity peaks at a few cycles per degree and falls steeply above roughly 10 cpd, reaching the resolution limit near 50 to 60 cpd. Lateral inhibition in center-surround receptive fields suppresses the lowest frequencies, and optical blur and the cone sampling mosaic limit the highest.
- **Chromatic:** low-pass, with a much lower cutoff. Mullen ("The contrast sensitivity of human colour vision to red-green and blue-yellow chromatic gratings", Journal of Physiology 359, 1985) measured isoluminant gratings whose sensitivity falls off several times earlier than luminance gratings; blue-yellow resolution is lower still.

Campbell and Robson ("Application of Fourier analysis to the visibility of gratings", Journal of Physiology 197, 1968) showed that the visibility of complex gratings follows from the responses of separate channels tuned to bands of spatial frequency, which is the physiological motivation for multiscale models. Mannos and Sakrison (1974) gave an early analytic luminance CSF used in image coding.

Two further effects matter for quantization. Sensitivity depends on adaptation level: at low luminance the CSF drops and its peak shifts to lower frequencies, and near black the visibility of a fixed luminance step changes with the display's black level and surround. Chromatic discrimination likewise declines as adaptation moves from photopic toward mesopic light, as described under [light level and adaptation](#light-level-and-adaptation). Contrast masking reduces the visibility of an error placed on a busy texture compared with the same error on a flat area, so identical error energy is more visible in skies and gradients than in foliage.

## From pixels to cycles per degree

Spatial frequency on the retina depends on display pitch and viewing distance:

```text
pixels per degree = d · tan(1°) / p          d = viewing distance, p = pixel pitch (same unit)
Nyquist frequency  = pixels per degree / 2    cycles per degree
```

A 100 ppi display (`p = 0.254 mm`) viewed at 50 cm gives about 34 pixels per degree, so a one-pixel checkerboard is about 17 cpd. A 220 ppi display at 40 cm gives about 60 pixels per degree and 30 cpd. In the first case the finest luminance pattern is attenuated but still visible, and the finest chroma pattern is close to invisible. In the second, the finest pattern is near the luminance resolution limit. Terminal image viewing adds a complication: users often enlarge character cells, which lowers the pixels per degree and moves the same dither pattern into the visible band.

## Spatial integration and noise visibility

At normal viewing distance the eye integrates fine detail. The contrast sensitivity functions above act as low-pass filters on what is seen: luminance contrast is attenuated above roughly 10 cpd, and chromatic contrast already above a few cpd. A pattern whose energy lies above these frequencies is perceived mainly through its local average, so its average tone and color matter far more than its individual elements. The same pattern seen closer, or with larger pixels, moves to lower retinal frequencies and becomes visible as texture.

This is the perceptual basis of dithering and halftoning: an image can be reproduced with few colors when the error introduced by quantization is placed at frequencies the viewer does not resolve, while the local averages match the original. Error energy at low frequencies is seen directly as blotches, contours, or color casts. Within the visible band, regular structure such as stripes and lattices tends to be more conspicuous than isotropic noise of the same energy, because the visual system is tuned to orientation and readily groups regular elements. [Noise spectrum and why dither noise belongs at high frequencies](metrics.md#noise-spectrum-and-why-dither-noise-belongs-at-high-frequencies) describes how dithers shape their error spectrum and how the metrics measure it.

## Luminance and chroma in spatial vision

The achromatic channel has the highest spatial resolution and contrast sensitivity, as described under [contrast sensitivity](#contrast-sensitivity). Structure, edges, and texture are perceived mainly through luminance contrast: an isoluminant image, whose regions differ only in chroma, looks blurry and its edges are hard to locate. Fine chromatic error is attenuated far more than fine luminance error, which is why image and video coding keep luma at full resolution while subsampling chroma, as in JPEG and 4:2:0 video, with little visible loss.

Chroma still matters at low spatial frequencies. Large areas are judged as colors, and color casts, banding in colored gradients, and tinted neutrals are visible even though fine chroma detail is not. Perception therefore favors luminance for *spatial structure* and chroma for *area color*, which is the division the metrics follow; see [why luminance is currently emphasized](metrics.md#why-luminance-is-currently-emphasized).

## References

- Aguilar, M., and W. S. Stiles. "Saturation of the rod mechanism of the retina at high levels of stimulation." Optica Acta 1(1), 1954.
- Campbell, F. W., and J. G. Robson. "Application of Fourier analysis to the visibility of gratings." Journal of Physiology 197, 1968.
- CIE Publication 191. Recommended system for mesopic photometry based on visual performance, 2010.
- Fairchild, M. D., and G. M. Johnson. "iCAM framework for image appearance, differences, and quality." Journal of Electronic Imaging 13(1), 2004.
- Hurvich, L. M., and D. Jameson. "An opponent-process theory of color vision." Psychological Review 64(6), 1957.
- ITU-R Recommendation BT.500. Methodologies for the subjective assessment of the quality of television images.
- Mannos, J. L., and D. J. Sakrison. "The effects of a visual fidelity criterion on the encoding of images." IEEE Transactions on Information Theory 20(4), 1974.
- Mullen, K. T. "The contrast sensitivity of human colour vision to red-green and blue-yellow chromatic gratings." Journal of Physiology 359, 1985.
- von Kries, J. "Chromatic adaptation." Festschrift der Albrecht-Ludwigs-Universität, Freiburg, 1902. English translation in D. L. MacAdam, ed., *Sources of Color Science*, MIT Press, 1970.
