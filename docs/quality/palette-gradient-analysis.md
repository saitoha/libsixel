# Palette Gradient Analysis

[`tools/palette_gradient.py`](../../tools/palette_gradient.py) turns a source image and an aligned quantized image into a selectable gradient analysis. It opens a local browser page, highlights source-derived candidates one at a time, and generates RGB palette scatter plots, exact Voronoi plane intersections, actual pixel profiles and nearest-color chord assignments for the selected line.

The tool is a local diagnostic. It does not rank whole-image perceptual quality, change an encoder policy, or infer the algorithm that created an image. Automatic candidates identify plausible smooth transitions; a user selects the transition relevant to the investigation.

## Run the command

Python 3.12 or later is required by the pinned analysis packages. These packages are optional and independent of the normal C build. From the repository root, prepare a private environment once:

```sh
python3 -m venv .local/palette-gradient-env
.local/palette-gradient-env/bin/python -m pip install -r tools/palette-gradient-requirements.txt
```

Supply the source and a lossless quantized image:

```sh
.local/palette-gradient-env/bin/python tools/palette_gradient.py \
  source.png quantized.png --output /tmp/palette-gradient
```

The command starts a loopback-only HTTP server and opens its URL in the default browser. The workflow is:

1. Select a highlighted candidate on the source image with the candidate menu or previous/next buttons.
2. If needed, select two endpoints on the image, or enter their original pixel coordinates in the expandable controls.
3. Generate the figures. The source selection, measurements, PNG/SVG figures and a standalone HTML report are saved in the output directory.

The coordinate controls also allow an explicit `R`, `G` or `B` slice. Stop the server with Ctrl-C after inspection; the report and figures remain usable without it. Use `--no-open` to print the URL without launching a browser, or `--port PORT` to choose its loopback port.

## Inputs and palette identity

Both images must have equal dimensions after applying EXIF orientation. The tool does not resize them or perform ICC color management. Use the source at the same processing boundary as the output: if encoding involved cropping, resizing, background composition or CMS, provide the corresponding normalized reference image. Input hashes, modes, dimensions, EXIF orientation and ICC-profile presence are recorded, together with the generator hash and Python/package versions.

The supported source layouts are 8-bit RGB, RGBA, indexed, grayscale and grayscale with alpha. Animated input must first be reduced to one matching frame. A selected line must consist entirely of opaque pixels in both source and output. The tool does not invent a background or mix transparent colors into its RGB geometry.

Palette recovery depends on the output image:

| Output representation | Sites used for geometry | Site IDs |
| --- | --- | --- |
| Indexed image | All opaque color-table entries, including unused ones | Original table indices |
| RGB or grayscale image | Unique colors actually present in opaque output pixels | Stable IDs in lexicographic RGB order |

Transparent or partially transparent palette entries are excluded. Coincident RGB entries remain in the recorded table, while their geometric cell is assigned to the first eligible entry. An RGB image cannot reveal an unused palette entry; the report explicitly scopes its geometry to observed colors. More than 256 eligible colors is an error, rather than a request to quantize the input again. Use PNG or another lossless format; JPEG artifacts can create extra colors.

The image palette contains the colors stored in the output image. A PNG decoded from SIXEL can therefore contain percentage-rounded RGB values that differ from a pre-serialization `-M` palette. The analysis preserves those actual output colors. It does not silently substitute encoder-internal entries or simulate a working-space transform. See [External palettes](../functionality/external-palettes.md), [Output color space](../functionality/output-colorspace.md) and [Quality measurement policy](measurement-policy.md).

## How automatic extraction works

Extraction uses only the source image. Output palette geometry and output errors do not affect candidate selection or ranking. The same source, ROI and parameters yield the same ordered candidates across compared outputs.

A working image with a maximum dimension of 512 pixels provides smoothed RGB spatial derivatives. A local color structure tensor estimates the dominant spatial direction of change. The detector scans several lengths along that direction and retains transitions with sufficient endpoint color separation, few reversals, small deviations from a straight color trajectory, and a transition spread across enough samples. The effective transition width rejects an isolated sharp edge even when downsampling or smoothing has softened it.

Candidates are ranked by source color span, effective transition width and straightness. Nearby parallel duplicates are suppressed. This heuristic does not guarantee that every physical gradient is found or that a photographic edge cannot pass its tests. Complex textures, curved spatial gradients, very short transitions and nearly flat regions can be missed. An empty candidate list is a valid result; endpoint selection remains available.

Smoothing and resizing are used for detection only. Saved endpoints are mapped back to original image coordinates. Analysis reads actual, unsmoothed pixels along the rasterized spatial line; it does not interpolate colors from the detection image.

Use `--roi x,y,width,height` to focus extraction and `--candidates N` to request up to 32 candidates. ROI coordinates refer to the oriented full-resolution source image.

## Geometry and interpretation

All geometry uses unweighted squared Euclidean distance in 8-bit RGB code values. Every recovered palette site competes in each nearest-color query, including sites omitted from the local 3D display for readability. The tool is not a model of non-RGB working-space lookup, approximate lookup, diffusion, alpha processing, or SIXEL emission.

Three related objects are displayed separately:

- **Actual spatial profile:** the source pixels encountered along the selected image line. The actual quantized output is sampled at exactly the same coordinates.
- **Endpoint chord:** the straight RGB segment connecting the first and last source colors. Its nearest-color intervals are calculated analytically from all competing halfspaces.
- **Plane projection:** a two-dimensional view of the source trajectory and the three-dimensional Voronoi cells. Projection can hide a component of the source variation, so the maximum and RMS off-plane distances are always reported.

An automatic slice fixes the least-changing RGB channel at its median when that channel varies little relative to the endpoint color separation. Otherwise the tool uses a plane containing the exact endpoint chord and fits its second direction to source variation perpendicular to that chord. A perfectly straight trajectory uses a deterministic orthogonal direction. All compared palettes share the same source-derived plane, plot bounds and 3D axis limits.

For a plane parameterized by an origin and two orthonormal basis vectors, the nearest-site score includes both in-plane distance and the squared perpendicular distance of the original RGB site from that plane. In a `G=252` slice, for example:

```text
d² = (R − pR)² + (B − pB)² + (252 − pG)²
```

Thus the projected site can lie outside its own cell in the slice. Drawing an ordinary 2D Voronoi diagram from projected points would omit the last term and yield incorrect boundaries. The tool intersects the full RGB halfspaces with the plane, the declared view rectangle, and the valid RGB cube. Cell area is a clipped two-dimensional area in RGB units squared, not full 3D volume or image pixel coverage.

Circles mark projected sites and numeric labels identify table entries. Sites outside the view can still own a cell inside it; the report's RGB table and `analysis.json` retain those sites and their coordinates. The actual-output profile can differ from exact nearest-color assignments because of diffusion, approximate lookup, output rounding or a different processing geometry. Such a difference is evidence to investigate, not automatically an encoder bug.

## Reuse a selection and compare outputs

To compare another quantizer without selecting a new source region:

```sh
.local/palette-gradient-env/bin/python tools/palette_gradient.py \
  source.png another-quantized.png \
  --selection /tmp/palette-gradient/selection.json \
  --output /tmp/palette-gradient-another
```

The source hash must match the saved selection. An explicit slice is preserved; an automatically chosen slice is deterministically reconstructed from the saved source line. `--slice G=252` overrides the saved choice.

Alternatively, compare up to three aligned outputs in one set of figures:

```sh
.local/palette-gradient-env/bin/python tools/palette_gradient.py \
  source.png heckbert.png --compare Ward=ward.png \
  --compare Kmeans=kmeans.png --output /tmp/palette-gradient-comparison
```

The browser displays the source and primary output during selection; generated figures include every supplied output.

Batch operation is available without starting a server:

```sh
# Use source-derived candidate number 3.
.local/palette-gradient-env/bin/python tools/palette_gradient.py \
  source.png quantized.png --candidate 3 --output /tmp/gradient-candidate

# Use explicit source coordinates and an explicit plane.
.local/palette-gradient-env/bin/python tools/palette_gradient.py \
  source.png quantized.png --line 570,302,570,284 --slice G=252 \
  --output /tmp/gradient-line
```

These coordinates are illustrative; they must lie inside the supplied image. Candidates and rankings are inspected in `candidates.json`; the chosen endpoints and source provenance are in `selection.json`.

## Output files

| File | Content |
| --- | --- |
| `01-selected-gradient.png` / `.svg` | Selected line on the source and each output |
| `02-gradient-profile.png` / `.svg` | Source pixels, actual output, exact-nearest output and channel curves |
| `03-rgb-3d.png` / `.svg` | RGB palette sites, actual source trajectory and endpoint chord |
| `04-voronoi-slice.png` / `.svg` | Full-distance Voronoi plane intersections and projected sites |
| `05-chord-assignment.png` / `.svg` | Nearest assignments and RGB error along the modeled endpoint chord |
| `analysis.json` | Full palettes, palette origins, sampled pixel coordinates, profiles, plane basis, cell polygons and analytic intervals |
| `candidates.json` / `selection.json` | Source-only extraction and reusable selection |
| `report.html` | Offline report, figures, RGB site table and interpretation boundaries |

Re-generating in the same output directory replaces these analysis artifacts. Use a different output directory to retain several selections.

## Validation

The optional tests run in the same analysis environment:

```sh
.local/palette-gradient-env/bin/python tests/tools/palette-gradient/run.py
```

They verify analytic chord intervals against direct distance queries, hidden-axis penalties, equal-score site ties, fitted-plane RGB-cube clipping, deterministic source-only extraction on a known ramp, rejection of flat fields/sharp steps/noise, exact pixel sampling, alpha exclusion, indexed palette identity and rejection of nonquantized output. Browser selection, coordinate entry, figure generation and responsive layout also require a visual check when their UI changes.

Neither these tests nor the tool's diagnostic RGB errors replace the encoder's normal `make staticcheck`, `make check`, or controlled `lsqa` quality measurements.
