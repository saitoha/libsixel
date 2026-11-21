# Acceleration ledger

This ledger is for contributors who look for a missing SIMD or GPU port, or who add one. It lists each accelerated operation with its state on every backend. The states, the `Accel:` tags that back them, and the static check are defined in [Acceleration backends](README.md). Code written before the ledger existed is not listed yet; it is frozen in the [legacy list](../../tests/_static/data/accel-untagged-legacy.tsv) until it is tagged.

## Ledger

Rows are sorted by ID. The prefix names the stage: `DECODE` for the SIXEL decoder, `SAMPLE` for adaptive sample extraction, `HECKBERT` for median-cut palette construction, `REFRESH` for spatial refresh detection, `RETAINED` for the retained canvas, `PUBLISH` for indexed-frame publication, `EMIT` for CPU SIXEL emission, and `BODY` for GPU SIXEL body composition.

<!-- accel-ledger -->

| ID | Operation | Scalar | NEON | SSE2 | AVX2 | WASM | Metal | Vulkan | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BODY-COMPOSE | Compose SIXEL body bands and spell their text on the GPU, so encode workers only copy bands | done | n/a | n/a | n/a | n/a | done | done | The Metal program also looks up the next span start from a column table. |
| DECODE-BAND-PARSE | Parse kept band-column bodies in chunks on the GPU under the decoder GPU policy | done | n/a | n/a | n/a | n/a | done | planned | |
| DECODE-BAND-WORDS | Band-word decoder helpers: load sixels, run extents, plane updates, gathers and RGBA blends | done | done | planned | planned | planned | n/a | n/a | The NEON helpers need AArch64 table lookups. |
| DECODE-BODY-SPLIT | Split a body at `$` and `-` into chunks for GPU parsing | done | done | planned | planned | n/a | n/a | n/a | x86 needs it only with a Vulkan `DECODE-BAND-PARSE`. |
| DECODE-FINALIZE | Final column pass of a decode, including the alpha test | done | done | planned | planned | planned | n/a | n/a | |
| DECODE-LITERAL-RUN | Parse literal runs sixteen bytes at a time and short numbers without a branch per digit | done | done | planned | planned | planned | n/a | n/a | |
| EMIT-BAND-CLASSIFY | Classify the colors present in each band | done | done | done | planned | planned | n/a | n/a | |
| EMIT-PUT-NODE | Copy node cells and find long runs while emitting a node | done | done | done | rejected | planned | n/a | n/a | A 32-lane AVX2 scan measured slower than SSE2: nodes are short and most lanes stay idle. |
| HECKBERT-BOX-BOUNDS | Box bounds and weight over RGB tuples | done | done | planned | planned | planned | n/a | n/a | |
| HECKBERT-BOX-SCORE | Squared-error score of a box over RGB tuples | done | done | planned | planned | planned | n/a | n/a | |
| HECKBERT-MEDIAN-CUT | Solve the median cut on the GPU | done | n/a | n/a | n/a | n/a | done | planned | |
| PUBLISH-INDEX-SCAN | Check published index rows for bytes that need correction | done | done | done | planned | planned | n/a | n/a | |
| REFRESH-COARSE | Reduce the activity map to 4x4 tiles | done | planned | planned | done | planned | n/a | n/a | |
| REFRESH-LUMA | Compute luma for spatial refresh | done | planned | planned | done | planned | n/a | n/a | |
| REFRESH-SOBEL | Sobel magnitude for spatial refresh | done | done | planned | done | planned | n/a | n/a | |
| RETAINED-EXPAND | Expand retained canvas rows on the GPU | done | n/a | n/a | n/a | n/a | done | done | |
| RETAINED-ROWS | Gather retained rows through the palette table | done | done | planned | done | planned | n/a | n/a | |
| SAMPLE-SOLID-ADD | Add solid sample points to their histogram cells | done | done | planned | planned | planned | n/a | n/a | |
| SAMPLE-VARY-POINTS | Test whether sample points lie in a solid region | done | done | done | done | planned | done | planned | Metal scans the whole frame on the device for large frames. |
| SAMPLE-VARY-RGB | Test RGB sample points through channel planes and table lookups | done | done | n/a | planned | planned | n/a | n/a | SSE2 has no byte shuffle; the AVX2 port uses `vpshufb`. |

<!-- accel-ledger-end -->

## Maintenance

Add a row when a new operation gets its first accelerated backend, and mark every other backend that should follow as `planned`. When a legacy unit is tagged, add its row here and remove it from the legacy list in the same change.
