# Acceleration ledger

This ledger is for contributors who look for a missing SIMD or GPU port, or who add one. It lists each accelerated operation with its state on every backend. The states, the `Accel:` tags that back them, and the static check are defined in [Acceleration backends](README.md). Code written before the ledger existed is not listed yet; it is frozen in the [legacy list](../../tests/_static/data/accel-untagged-legacy.tsv) until it is tagged.

## Ledger

Rows are sorted by ID. The prefix names the stage: `DECODE` for the SIXEL decoder, `SAMPLE` for adaptive sample extraction, `HECKBERT` for median-cut palette construction, `KMEANS` for k-means palette construction and application, `REFRESH` for spatial refresh detection, `RETAINED` for the retained canvas, `PUBLISH` for indexed-frame publication, `EMIT` for CPU SIXEL emission, and `BODY` for GPU SIXEL body composition.

<!-- accel-ledger -->

| ID | Operation | Scalar | NEON | SSE2 | AVX2 | WASM | Metal | Vulkan | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BODY-COMPOSE | Build occupancy, spans, cell values, and first-seen register order for SIXEL body bands on the GPU | done | n/a | n/a | n/a | n/a | done | done | |
| BODY-SPELL | Spell composed SIXEL spans into body bytes on the GPU, so encode workers only copy bands | done | n/a | n/a | n/a | n/a | done | done | The grouped order stage looks up the next span start from a column table when the encoded width is at most 2048. Vulkan starts on the CPU and uses the shared measured GPU headroom rule: windows of at least 8 frames and 20 ms, averaged load below 0.50 to enter GPU spelling and above 0.70 to return to CPU emission. GPU policy off keeps CPU emission. |
| DECODE-BAND-PARSE | Parse kept band-column bodies in chunks on the GPU under the decoder GPU policy | done | n/a | n/a | n/a | n/a | done | planned | |
| DECODE-BAND-WORDS | Band-word decoder helpers: load sixels, run extents, plane updates, gathers and RGBA blends | done | done | planned | planned | planned | n/a | n/a | The NEON helpers need AArch64 table lookups. |
| DECODE-BODY-SPLIT | Find the end of a SIXEL payload run and collect chunk boundaries for GPU parsing | done | done | planned | planned | n/a | n/a | n/a | x86 needs it only with a Vulkan `DECODE-BAND-PARSE`. |
| DECODE-FINALIZE | Final column pass of a decode, including the alpha test | done | done | planned | planned | planned | n/a | n/a | |
| DECODE-LITERAL-RUN | Parse literal runs sixteen bytes at a time and short numbers without a branch per digit | done | done | planned | planned | planned | n/a | n/a | |
| EMIT-BAND-CLASSIFY | Classify the colors present in each band | done | done | done | planned | planned | n/a | n/a | |
| EMIT-COUNT-TABLE | Spell decimal counts and color selectors from a constant byte table | done | n/a | n/a | n/a | n/a | n/a | n/a | Portable scalar table lookup; GPU body spelling is covered by BODY-SPELL. |
| EMIT-PUT-NODE | Copy node cells and find long runs while emitting a node | done | done | done | rejected | planned | n/a | n/a | A 32-lane AVX2 scan measured slower than SSE2: nodes are short and most lanes stay idle. |
| HECKBERT-BOX-BOUNDS | Box bounds and weight over RGB tuples | done | done | planned | done | planned | n/a | n/a | |
| HECKBERT-BOX-SCORE | Squared-error score of a box over RGB tuples | done | done | planned | done | planned | n/a | n/a | |
| HECKBERT-MEDIAN-CUT | Solve the median cut on the GPU | done | n/a | n/a | n/a | n/a | done | done | Vulkan uses a separate compute queue when available and the shared measured GPU headroom rule, with CPU median cut as the fallback. |
| HECKBERT-RESIDENT-REP | Order SSE boxes and compute RGB888 representatives in the resident palette path | done | n/a | n/a | n/a | n/a | done | planned | |
| ICC-PRODUCT-RESIDUAL | Recover the double-precision product residual for ICC arithmetic | done | n/a | done | n/a | n/a | n/a | n/a | The x86 128-bit implementation requires FMA3 and OS AVX support. |
| KMEANS-RESIDENT-COVER | Select soft or hard RGB888 anchors and repair the palette before lookup | done | n/a | n/a | n/a | n/a | done | planned | |
| KMEANS-RESIDENT-HISTOGRAM | Sample the adaptive grid and compact weighted RGB888 histogram bins | done | n/a | n/a | n/a | n/a | done | planned | |
| KMEANS-RESIDENT-LOOKUP | Assign final pixel indexes with the resident 6bit palette table | done | n/a | n/a | n/a | n/a | done | planned | |
| KMEANS-RESIDENT-OBJECTIVE | Score final weighted samples against the resident centers | done | n/a | n/a | n/a | n/a | done | planned | |
| KMEANS-RESIDENT-PACK | Pack final byte centers into a resident RGB888 palette | done | n/a | n/a | n/a | n/a | done | planned | |
| KMEANS-RESIDENT-PCA | Compute the principal axis and seed centers from weighted projection buckets | done | n/a | n/a | n/a | n/a | done | planned | |
| KMEANS-RESIDENT-SNAP | Apply the final byte snap map before RGB888 cover repair | done | n/a | n/a | n/a | n/a | done | planned | |
| PUBLISH-INDEX-SCAN | Check published index rows for bytes that need correction | done | done | done | planned | planned | n/a | n/a | |
| REFRESH-COARSE | Reduce the activity map to 4x4 tiles | done | planned | planned | done | planned | n/a | n/a | |
| REFRESH-LUMA | Compute luma for spatial refresh | done | planned | planned | done | planned | n/a | n/a | |
| REFRESH-SOBEL | Sobel magnitude for spatial refresh | done | done | planned | done | planned | n/a | n/a | |
| RETAINED-EXPAND | Expand retained canvas rows on the GPU | done | n/a | n/a | n/a | n/a | done | done | |
| RETAINED-ROWS | Gather retained rows through the palette table | done | done | planned | done | planned | n/a | n/a | |
| SAMPLE-SOLID-ADD | Add solid sample points to their histogram cells | done | done | planned | rejected | planned | n/a | n/a | The AVX2 RGB update raised self Ir from 25 to 32 instructions/point; the sample worker plus its AVX2 helper used 5.6% more instructions (Core Ultra 5 235). |
| SAMPLE-VARY-POINTS | Test whether sample points lie in a solid region | done | done | done | done | planned | done | done | Metal scans large frames on the device; Vulkan scans when recent GPU busy time leaves clear headroom, uploads a separate palette source when needed, and shares the input upload with dither when possible. |
| SAMPLE-VARY-RGB | Test RGB sample points through channel planes and table lookups | done | done | n/a | done | planned | n/a | n/a | SSE2 has no byte shuffle; the AVX2 port uses `vpshufb`. |

<!-- accel-ledger-end -->

## Maintenance

Add a row when a new operation gets its first accelerated backend, and mark every other backend that should follow as `planned`. When a legacy unit is tagged, add its row here and remove it from the legacy list in the same change.
