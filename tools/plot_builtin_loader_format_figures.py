#!/usr/bin/env python3
"""Generate implementation-level pipeline maps for builtin image formats."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Sequence

from plot_encoding_pipeline_figures import (
    BLUE,
    BLUE_LIGHT,
    GOLD,
    GOLD_LIGHT,
    GRID,
    INK,
    MAGENTA,
    MAGENTA_LIGHT,
    MUTED,
    PANEL,
    PAPER,
    TEAL,
    TEAL_LIGHT,
    path,
    rect,
    svg_document,
    text,
    text_lines,
)


PURPLE = "#6941C6"
PURPLE_LIGHT = "#F4EBFF"
RED = "#B42318"
RED_LIGHT = "#FEE4E2"
SLATE_LIGHT = "#EEF2F6"

OUTPUT_DIR = Path("docs/loader/builtin/pipeline-figures")
MANIFEST_NAME = "builtin-loader-pipelines.json"


def stage(coverage_id: str, phase: str, title_value: str,
          source: str, symbols: Sequence[str], details: Sequence[str],
          role: str) -> dict[str, object]:
    """Return one pipeline stage definition."""
    return {
        "coverage_id": coverage_id,
        "phase": phase,
        "title": title_value,
        "source": source,
        "symbols": list(symbols),
        "details": list(details),
        "role": role,
    }


PIPELINES: dict[str, dict[str, object]] = {
    "sixel": {
        "title": "Builtin SIXEL input pipeline",
        "subtitle": "DCS recognition, paint state, optional workers, typed frame",
        "document": "docs/loader/builtin/sixel.md",
        "stages": [
            stage("SIX-01", "ROUTE", "Recognize one leading DCS",
                  "src/loader-builtin.c",
                  ("sixel_builtin_detect_decode_path",),
                  ("ESC P or C1 DCS; scan parameters to final q",
                   "select the dedicated SIXEL branch"), "route"),
            stage("SIX-02", "PARSE", "Initialize raw decoder state",
                  "src/fromsixel.c",
                  ("sixel_decode_raw", "sixel_decode_raw_impl",
                   "parser_context_enter_decsixel"),
                  ("validate header, raster extent, repeats and cursor motion",
                   "allocate index canvas and local register table"), "parse"),
            stage("SIX-03", "PAINT", "Resolve registers and paint commands",
                  "src/fromsixel.c",
                  ("hls_to_rgb", "image_buffer_store_pixel",
                   "image_buffer_ormode_store_sixel"),
                  ("RGB/HLS definitions update local registers",
                   "overwrite or P2=5 OR-mode updates the index canvas"),
                  "decode"),
            stage("SIX-04", "PARALLEL", "Validate spans before worker paint",
                  "src/decoder-parallel.c",
                  ("sixel_decoder_parallel_request_start",
                   "sixel_decoder_parallel_direct_rows_disjoint"),
                  ("release workers only after anchored, disjoint ownership",
                   "unsafe split or create failure returns to clean serial paint"),
                  "branch"),
            stage("SIX-05", "OUTPUT", "Choose PAL8 or expand final palette",
                  "src/loader-builtin.c",
                  ("sixel_builtin_load_with_builtin_impl",
                   "sixel_builtin_finalize_loaded_frame"),
                  ("palette fusion keeps byte indexes when permitted",
                   "otherwise emit gamma RGB888; no mask is requested"),
                  "output"),
        ],
        "tests": {
            "SIX-01": ["tests/processing/geometry/0009_sixel_resize_palette_limit.t"],
            "SIX-02": ["tests/processing/decoder/0002_decoder_ormode_raw_overlay.t"],
            "SIX-03": ["tests/processing/decoder/0005_decoder_ormode_repeat_overlay.t"],
            "SIX-04": ["tests/processing/decoder/0001_decoder_parallel_split_after_newline.t"],
            "SIX-05": [
                "tests/loader/builtin/1986_loader_builtin_sixel_unpainted_numeric.t",
                "tests/loader/builtin/1987_loader_builtin_sixel_high_color_numeric.t",
            ],
        },
    },
    "netpbm": {
        "title": "Builtin Netpbm pipeline",
        "subtitle": "P1-P7 grammar, typed samples, linear alpha composition",
        "document": "docs/loader/builtin/netpbm.md",
        "stages": [
            stage("PNM-01", "ROUTE", "Recognize P1 through P7",
                  "src/loader-builtin.c",
                  ("sixel_builtin_detect_decode_path",),
                  ("require magic plus following whitespace",
                   "route the complete chunk to load_pnm"), "route"),
            stage("PNM-02", "HEADER", "Parse grammar and tuple model",
                  "src/frompnm.c",
                  ("pnm_parse_header", "pnm_parse_tuple_type",
                   "pnm_validate_header_limits"),
                  ("select ASCII/binary reader, depth, alpha and MAXVAL",
                   "apply strict or compatibility header policy"), "parse"),
            stage("PNM-03", "SAMPLES", "Decode and validate raster samples",
                  "src/frompnm.c",
                  ("pnm_reader_read_sample", "pnm_validate_sample",
                   "pnm_validate_raster_tail"),
                  ("PBM inversion, gray replication and MAXVAL normalization",
                   "reject malformed samples, truncation and trailing data"),
                  "decode"),
            stage("PNM-04", "PRECISION", "Select byte or float output",
                  "src/frompnm.c",
                  ("pnm_decode_rgb8_noalpha",
                   "pnm_decode_rgbfloat_noalpha"),
                  ("MAXVAL <= 255 becomes gamma RGB888",
                   "MAXVAL > 255 becomes gamma RGBFLOAT32"), "branch"),
            stage("PNM-05", "ALPHA", "Composite PAM alpha in linear light",
                  "src/frompnm.c",
                  ("pnm_compose_rgba8_to_linearrgbfloat32",
                   "pnm_compose_rgbafloat_to_linearrgbfloat32"),
                  ("decode source and background transfer functions",
                   "return opaque LINEARRGBFLOAT32"), "output"),
        ],
        "tests": {
            "PNM-01": ["tests/loader/builtin/1546_loader_builtin_pnm_pam_tupletype_matrix_numeric.t"],
            "PNM-02": ["tests/loader/builtin/1296_loader_builtin_pnm_ppm8_fastpath_numeric.t"],
            "PNM-03": ["tests/loader/builtin/1548_loader_builtin_pnm_binary_truncation_matrix_numeric.t"],
            "PNM-04": ["tests/loader/builtin/1295_loader_builtin_pnm_ppm16_float32_numeric.t"],
            "PNM-05": ["tests/loader/builtin/1294_loader_builtin_pnm_pam_rgba_linear_bg_numeric.t"],
        },
    },
    "gif": {
        "title": "Builtin GIF pipeline",
        "subtitle": "Block scan, LZW, canvas disposal, loop-aware frame emission",
        "document": "docs/loader/builtin/gif.md",
        "stages": [
            stage("GIF-01", "ROUTE", "Recognize GIF87a or GIF89a",
                  "src/loader-builtin.c",
                  ("sixel_builtin_load_gif_frames",),
                  ("initialize callback-owned frame finalization",
                   "pass start-frame and loop controls to fromgif"), "route"),
            stage("GIF-02", "SCAN", "Read logical screen and extensions",
                  "src/fromgif.c",
                  ("gif_load_header", "gif_scan_stream_info",
                   "gif_skip_subblocks"),
                  ("resolve global/local tables, GCE, NETSCAPE loop data",
                   "ignore structurally valid unknown extensions"), "parse"),
            stage("GIF-03", "LZW", "Decode image raster into frame indices",
                  "src/fromgif.c",
                  ("gif_process_raster", "gif_out_code"),
                  ("expand clear/end codes and interlaced row order",
                   "normalize transparent index and dirty regions"), "decode"),
            stage("GIF-04", "COMPOSE", "Apply frame rectangle and disposal",
                  "src/fromgif.c",
                  ("gif_decode_one_frame", "gif_history_mark",
                   "gif_reset_canvas_for_loop"),
                  ("compose over persistent canvas",
                   "restore background or saved previous pixels"), "branch"),
            stage("GIF-05", "EMIT", "Select frame, loop and output form",
                  "src/fromgif.c",
                  ("gif_should_emit_decoded_frame",
                   "gif_export_pal8_frame", "gif_export_nonpal_frame"),
                  ("honor start-frame and loop policy",
                   "emit PAL8 when stable or RGB/mask when composition requires"),
                  "output"),
        ],
        "tests": {
            "GIF-01": ["tests/loader/builtin/0044_builtin_gif_start_frame_positive.t"],
            "GIF-02": ["tests/loader/builtin/0261_loader_builtin_gif_unknown_extension_ignored.t"],
            "GIF-03": ["tests/loader/builtin/0259_loader_builtin_gif_lct_gct_switch_frame2_lsqa.t"],
            "GIF-04": ["tests/loader/builtin/0234_loader_builtin_gif_transparency_dispose3_frame2_lsqa.t"],
            "GIF-05": ["tests/loader/builtin/0265_loader_builtin_gif_loop_auto_loop2.t"],
        },
    },
    "png": {
        "title": "Builtin PNG and APNG pipeline",
        "subtitle": "Chunk policy, inflate/unfilter, profile transforms, animation canvas",
        "document": "docs/loader/builtin/png.md",
        "stages": [
            stage("PNG-01", "ROUTE", "Classify static PNG or APNG",
                  "src/loader-builtin.c",
                  ("sixel_builtin_load_stbi_png_path",
                   "sixel_builtin_chunk_has_apng_control"),
                  ("validate the PNG signature and detect acTL",
                   "split still-image and animation control flow"), "route"),
            stage("PNG-02", "INDEXED", "Attempt palette-preserving decode",
                  "src/loader-builtin.c",
                  ("sixel_builtin_try_load_indexed_png",
                   "sixel_builtin_load_png_keycolor_or_rgba"),
                  ("decode PLTE/tRNS and enforce palette-fusion limits",
                   "choose PAL8 key color or RGBA fallback"), "branch"),
            stage("PNG-03", "RASTER", "Inflate, unfilter and deinterlace",
                  "src/frompng.c",
                  ("sixel_frompng_load_nonindexed",),
                  ("use adapted PNG/zlib helpers for scanline reconstruction",
                   "preserve 16-bit values as float before later conversion"),
                  "decode"),
            stage("PNG-04", "COLOR", "Resolve profile and transfer metadata",
                  "src/frompng.c",
                  ("sixel_frompng_build_profile_from_chunks",
                   "sixel_frompng_apply_colorspace_fallback_internal"),
                  ("apply iCCP/sRGB/cHRM/gAMA precedence",
                   "transform pixels and background in the matching precision"),
                  "color"),
            stage("PNG-05", "ANIMATE", "Rebuild and composite APNG frames",
                  "src/loader-builtin.c",
                  ("sixel_builtin_apng_process_chunk",
                   "sixel_builtin_apng_blend_rect",
                   "sixel_builtin_apng_emit_pending_frame"),
                  ("decode frame PNGs to RGBA8, not the static float path",
                   "integer canvas blend/dispose; then emission and background"), "decode"),
            stage("PNG-06", "OUTPUT", "Finalize alpha, orientation and frame",
                  "src/loader-builtin.c",
                  ("sixel_builtin_finalize_frame_callback",
                   "sixel_builtin_finalize_loaded_frame"),
                  ("convert temporary alpha to background or mask semantics",
                   "deliver typed pixels, colorspace and timing"), "output"),
        ],
        "tests": {
            "PNG-01": ["tests/loader/builtin/0017_apng_builtin_static_option.t"],
            "PNG-02": ["tests/loader/builtin/0143_loader_builtin_png_trns_keycolor_default_enabled_for_palette.t"],
            "PNG-03": ["tests/loader/builtin/0166_loader_builtin_trns_keycolor_optin_changes_rgba16_output.t"],
            "PNG-04": ["tests/loader/builtin/1977_loader_builtin_png_icc_numeric.t"],
            "PNG-05": ["tests/loader/builtin/1962_loader_builtin_png_apng_dispose_previous_numeric.t"],
            "PNG-06": ["tests/loader/builtin/1668_loader_builtin_png_orientation_toggle.t"],
        },
    },
    "jpeg": {
        "title": "Builtin JPEG pipeline",
        "subtitle": "Marker parser, entropy paths, precision-aware color conversion",
        "document": "docs/loader/builtin/jpeg.md",
        "stages": [
            stage("JPG-01", "ROUTE", "Recognize SOI and select JPEG loader",
                  "src/loader-builtin.c",
                  ("sixel_builtin_load_jpeg_frame",),
                  ("inspect precision/CMS policy before choosing an API",
                   "use byte fast path only when it preserves the contract"),
                  "route"),
            stage("JPG-02", "MARKERS", "Parse frame, tables and scans",
                  "src/stb_image.h",
                  ("stbi__decode_jpeg_header", "stbi__process_marker",
                   "stbi__process_scan_header"),
                  ("build quantization and Huffman state",
                   "reject unsupported arithmetic coding"), "parse"),
            stage("JPG-03", "ENTROPY", "Decode sequential/progressive/lossless data",
                  "src/stb_image.h",
                  ("stbi__parse_entropy_coded_data",
                   "stbi__parse_entropy_coded_data_lossless"),
                  ("inverse-DCT coefficient paths or lossless predictors",
                   "retain 9-16 bit samples for float output"), "decode"),
            stage("JPG-04", "COLOR", "Upsample and interpret component model",
                  "src/stb_image.h",
                  ("load_jpeg_image_float_high_precision",
                   "stbi__YCbCr_to_RGB_row_f32"),
                  ("handle Gray, RGB, YCbCr, CMYK and YCCK conventions",
                   "produce gamma RGB bytes or normalized float RGB"), "color"),
            stage("JPG-05", "PROFILE", "Apply embedded RGB ICC when enabled",
                  "src/loader-builtin.c",
                  ("sixel_builtin_extract_jpeg_icc",
                   "sixel_cms_convert_profile_to_srgb"),
                  ("assemble APP2 ICC fragments",
                   "transform float RGB; skip non-RGB profiles"), "color"),
            stage("JPG-06", "OUTPUT", "Apply orientation and deliver one frame",
                  "src/loader-builtin.c",
                  ("sixel_builtin_finalize_loaded_frame",),
                  ("optionally apply Exif orientation in common finalization",
                   "emit RGB888 or RGBFLOAT32 with gamma colorspace"), "output"),
        ],
        "tests": {
            "JPG-01": ["tests/loader/builtin/1976_loader_builtin_jpeg_rgb8_sequential_digest.t"],
            "JPG-02": ["tests/loader/builtin/0131_loader_builtin_rejects_arithmetic_jpeg.t"],
            "JPG-03": ["tests/loader/builtin/1983_loader_builtin_jpeg_rgb16_lossless_numeric.t"],
            "JPG-04": ["tests/loader/builtin/0123_loader_builtin_jpeg_ycck_8bit_seq444_r0_expected_lsqa.t"],
            "JPG-05": ["tests/loader/builtin/1978_loader_builtin_jpeg_icc_numeric.t"],
            "JPG-06": ["tests/loader/builtin/1667_loader_builtin_jpeg_orientation_toggle.t"],
        },
    },
    "hdr": {
        "title": "Builtin Radiance HDR pipeline",
        "subtitle": "Header hints, RGBE/XYZE scanlines, linear-light postprocess",
        "document": "docs/loader/builtin/hdr.md",
        "stages": [
            stage("HDR-01", "ROUTE", "Recognize Radiance signature and format",
                  "src/fromhdr.c",
                  ("sixel_builtin_load_hdr_frame",
                   "sixel_builtin_parse_hdr_profile_hint"),
                  ("accept #?RADIANCE or #?RGBE headers",
                   "collect FORMAT, GAMMA, PRIMARIES and exposure hints"),
                  "route"),
            stage("HDR-02", "GEOMETRY", "Parse resolution and orientation",
                  "src/fromhdr.c",
                  ("sixel_builtin_hdr_parse_resolution_line",
                   "sixel_builtin_hdr_map_scan_position"),
                  ("validate axis order, signs, dimensions and scan geometry",
                   "map file scan positions into canonical rows and columns"),
                  "parse"),
            stage("HDR-03", "SCANLINES", "Decode new RLE, old RLE or legacy stream",
                  "src/fromhdr.c",
                  ("sixel_builtin_hdr_decode_new_rle_scanline",
                   "sixel_builtin_hdr_decode_old_scanline",
                   "sixel_builtin_hdr_decode_legacy_stream"),
                  ("reconstruct four exponent-coded channels",
                   "validate run counts, truncation and scanline width"), "decode"),
            stage("HDR-04", "LINEARIZE", "Convert RGBE or XYZE to linear RGB float",
                  "src/fromhdr.c",
                  ("sixel_builtin_hdr_rgbe_to_float",
                   "sixel_builtin_hdr_xyz_to_linearrgb"),
                  ("restore shared exponent magnitude",
                   "convert XYZE through the built-in XYZ matrix"), "color"),
            stage("HDR-05", "POST", "Apply source profile and dynamic range policy",
                  "src/fromhdr.c",
                  ("sixel_builtin_hdr_apply_source_profile",
                   "sixel_builtin_hdr_apply_dynamic_range",
                   "sixel_builtin_hdr_apply_postprocess"),
                  ("use PRIMARIES/GAMMA or fallback profile",
                   "combine header exposure, EV and tone mapping"), "color"),
            stage("HDR-06", "OUTPUT", "Assign typed frame and optional target conversion",
                  "src/fromhdr.c",
                  ("sixel_builtin_hdr_assign_decoded_frame",),
                  ("start as LINEARRGBFLOAT32",
                   "CMS target may convert frame precision/colorspace"), "output"),
        ],
        "tests": {
            "HDR-01": ["tests/loader/builtin/0278_loader_builtin_hdr_cms_header_priority_numeric.t"],
            "HDR-02": ["tests/loader/builtin/0671_loader_builtin_hdr_orientation_all_numeric.t"],
            "HDR-03": [
                "tests/loader/builtin/1973_loader_builtin_hdr_new_rle_numeric.t",
                "tests/loader/builtin/1974_loader_builtin_hdr_old_scanline_rle_numeric.t",
                "tests/loader/builtin/1975_loader_builtin_hdr_legacy_stream_numeric.t",
            ],
            "HDR-04": ["tests/loader/builtin/0308_loader_builtin_hdr_numeric_target_linear_gamma_none_primaries_none_ev_0_tonemap_none_cms_none_fallback_linear_srgb.t"],
            "HDR-05": ["tests/loader/builtin/0534_loader_builtin_hdr_header_exposure_multi_numeric.t"],
            "HDR-06": ["tests/loader/builtin/0276_loader_builtin_hdr_cms_target_din99d.t"],
        },
    },
    "psd": {
        "title": "Builtin PSD and PSB pipeline",
        "subtitle": "Container validation, planar codecs, color modes, layer fallback",
        "document": "docs/loader/builtin/psd.md",
        "stages": [
            stage("PSD-01", "HEADER", "Parse PSD/PSB document structure",
                  "src/frompsd-header.c",
                  ("sixel_builtin_parse_psd_info",
                   "sixel_builtin_validate_psd_info"),
                  ("validate 8BPS version, depth, mode and section lengths",
                   "locate resources, composite and layer/mask windows"), "parse"),
            stage("PSD-02", "PLANES", "Decode composite channel payloads",
                  "src/frompsd.c",
                  ("sixel_builtin_decode_psd_8bit_channel",
                   "sixel_builtin_decode_psd_16bit_channel",
                   "sixel_builtin_decode_psd_32bit_channel"),
                  ("select raw, PackBits RLE, ZIP or ZIP prediction",
                   "retain native precision in planar working buffers"), "decode"),
            stage("PSD-03", "COLOR", "Interpret bitmap, Gray, Indexed, RGB, CMYK or Lab",
                  "src/loader-builtin.c",
                  ("sixel_builtin_psd_lookup_basic_decode_fn",
                   "sixel_builtin_psd_decode_cmyk_by_mode"),
                  ("dispatch mode/depth-specific conversion",
                   "produce RGB, linear RGB or CIELAB typed pixels"), "color"),
            stage("PSD-04", "FALLBACK", "Reconstruct a missing composite from layers",
                  "src/frompsd.c",
                  ("sixel_builtin_decode_psd_multilayer_missing_composite",
                   "sixel_builtin_psd_composite_layer_over"),
                  ("decode layer channels, masks, fills and supported effects",
                   "blend visible layers into a linear canvas"), "branch"),
            stage("PSD-05", "PROFILE", "Apply embedded ICC where applicable",
                  "src/loader-builtin.c",
                  ("sixel_builtin_psd_apply_embedded_icc",),
                  ("match profile colorspace to decoded mode",
                   "preserve explicit fallback when conversion is unavailable"),
                  "color"),
            stage("PSD-06", "OUTPUT", "Finalize alpha/mask and typed frame",
                  "src/loader-builtin.c",
                  ("sixel_builtin_load_psd_single_frame",
                   "sixel_builtin_finalize_loaded_frame"),
                  ("attach pixel buffer, colorspace and transparent mask",
                   "apply common orientation/alpha policy and emit once"), "output"),
        ],
        "tests": {
            "PSD-01": ["tests/loader/builtin/0177_loader_builtin_psd_spec_wrong_channel_count_reject.t"],
            "PSD-02": [
                "tests/loader/builtin/1984_loader_builtin_psd_rgb16_zip_pred_numeric.t",
                "tests/loader/builtin/1985_loader_builtin_psd_rgb32_rle_numeric.t",
            ],
            "PSD-03": ["tests/loader/builtin/0580_loader_builtin_psd_mode7_rgb32_rle_decode.t"],
            "PSD-04": ["tests/loader/builtin/0647_loader_builtin_psd_missing_composite_rgb8_multilayer_normal_decode.t"],
            "PSD-05": ["tests/loader/builtin/1979_loader_builtin_psd_icc_digest.t"],
            "PSD-06": ["tests/loader/builtin/0173_loader_builtin_psd_gray_duotone_16bit_alpha_bgcolor_composite.t"],
        },
    },
    "bmp": {
        "title": "Builtin BMP pipeline",
        "subtitle": "DIB dialects, raster codecs, nested payloads, color and alpha",
        "document": "docs/loader/builtin/bmp.md",
        "stages": [
            stage("BMP-01", "HEADER", "Probe file header and DIB dialect",
                  "src/frombmp-parser.c",
                  ("sixel_frombmp_probe", "sixel_bmp_parse_header"),
                  ("distinguish OS/2 and Windows structures",
                   "validate offsets, masks, palette and compression tuple"), "parse"),
            stage("BMP-02", "RASTER", "Decode native BMP pixels",
                  "src/frombmp.c",
                  ("sixel_frombmp_load",
                   "sixel_bmp_decode_indexed_uncompressed",
                   "sixel_bmp_decode_truecolor"),
                  ("expand palette or bitfields with row padding/origin",
                   "preserve explicit alpha when present"), "decode"),
            stage("BMP-03", "COMPRESSED", "Dispatch RLE and OS/2 compressed dialects",
                  "src/frombmp.c",
                  ("sixel_bmp_decode_rle8_rgb",
                   "sixel_bmp_decode_rle4_rgb",
                   "sixel_bmp_decode_huffman1d_rgb",
                   "sixel_bmp_decode_rle24_rgb"),
                  ("interpret encoded/absolute/control records",
                   "reject top-down or overrun combinations that are invalid"),
                  "branch"),
            stage("BMP-04", "NESTED", "Decode BI_JPEG or BI_PNG payload",
                  "src/loader-builtin.c",
                  ("sixel_builtin_load_nonpng_rgb8_fallback",
                   "sixel_builtin_load_jpeg_frame",
                   "sixel_frompng_load_nonindexed"),
                  ("slice the embedded payload from the parent BMP",
                   "reuse JPEG/PNG precision, CMS and alpha paths"), "branch"),
            stage("BMP-05", "COLOR", "Apply V4/V5 calibrated or embedded profile",
                  "src/loader-builtin.c",
                  ("sixel_builtin_apply_bmp_icc_to_rgba_channels",
                   "sixel_builtin_apply_bmp_calibrated_to_rgba_channels"),
                  ("choose embedded ICC before calibrated endpoints",
                   "convert RGB channels while retaining alpha"), "color"),
            stage("BMP-06", "OUTPUT", "Normalize legacy alpha and emit typed frame",
                  "src/loader-builtin.c",
                  ("sixel_builtin_apply_bmp_alpha_policy",
                   "sixel_builtin_finalize_loaded_frame"),
                  ("rescue all-zero implicit alpha only where permitted",
                   "composite or create transparency mask, then emit"), "output"),
        ],
        "tests": {
            "BMP-01": ["tests/loader/builtin/1414_loader_builtin_bmp_os2_rgb24_decode_numeric.t"],
            "BMP-02": ["tests/loader/builtin/1332_loader_builtin_bmp_rgba_mask_no_bg_numeric.t"],
            "BMP-03": ["tests/loader/builtin/1345_loader_builtin_bmp_rle4_mixed_numeric.t"],
            "BMP-04": ["tests/loader/builtin/1456_loader_builtin_bmp_bi_png16_alpha_bgcolor_cms_on_numeric.t"],
            "BMP-05": ["tests/loader/builtin/1372_loader_builtin_bmp_v5_embedded_icc_rgb_cms_on_numeric.t"],
            "BMP-06": ["tests/loader/builtin/1331_loader_builtin_bmp_rgba_bgcolor_float32_numeric.t"],
        },
    },
    "webp": {
        "title": "Builtin WebP pipeline",
        "subtitle": "RIFF plan, VP8/VP8L/ALPH codecs, metadata and animation",
        "document": "docs/loader/builtin/webp.md",
        "stages": [
            stage("WBP-01", "CONTAINER", "Parse RIFF chunks and build decode plan",
                  "src/fromwebp-container.c",
                  ("sixel_webp_parse_container",
                   "sixel_webp_build_decode_plan"),
                  ("validate VP8X flags, chunk graph, sizes and padding",
                   "classify VP8, VP8+ALPH, VP8L or animation"), "parse"),
            stage("WBP-02", "LOSSY", "Decode VP8 key frame",
                  "src/fromwebp-vp8.c",
                  ("sixel_webp_decode_vp8_payload",),
                  ("parse partitions, macroblocks, transforms and filters",
                   "upsample YUV into gamma RGBA"), "decode"),
            stage("WBP-03", "LOSSLESS", "Decode VP8L transform graph",
                  "src/fromwebp-vp8l.c",
                  ("sixel_webp_decode_stream",
                   "sixel_webp_decode_vp8l_payload"),
                  ("decode prefix codes, cache and back references",
                   "reverse predictor, color, green and index transforms"),
                  "decode"),
            stage("WBP-04", "ALPHA", "Decode and attach separate ALPH plane",
                  "src/fromwebp-vp8-alpha.c",
                  ("sixel_webp_apply_vp8_alpha_payload",
                   "sixel_webp_vp8_alpha_reconstruct"),
                  ("decode raw or VP8L-compressed alpha",
                   "reverse supported filters and copy into RGBA"), "branch"),
            stage("WBP-05", "ANIMATION", "Decode, composite and dispose ANMF frames",
                  "src/fromwebp.c",
                  ("sixel_fromwebp_load_animation",
                   "sixel_webp_anim_composite_rect",
                   "sixel_webp_anim_clear_rect"),
                  ("decode nested frame payloads into a canvas",
                   "honor blend, dispose, duration, start-frame and loop"), "decode"),
            stage("WBP-06", "META/OUTPUT", "Apply ICC, orientation and alpha policy",
                  "src/fromwebp.c",
                  ("sixel_webp_apply_iccp_to_srgb_rgba",
                   "sixel_webp_try_apply_exif_orientation",
                   "sixel_fromwebp_load"),
                  ("apply metadata precedence with parse-size limits",
                   "emit RGBA for common finalization or completed frames"),
                  "output"),
        ],
        "tests": {
            "WBP-01": ["tests/loader/builtin/1620_loader_builtin_webp_bad_chunk_payload_exceeds_riff_code.t"],
            "WBP-02": ["tests/loader/builtin/1980_loader_builtin_webp_vp8_digest.t"],
            "WBP-03": ["tests/loader/builtin/1931_loader_builtin_webp_vp8l_transform_subsample_quality_msssim.t"],
            "WBP-04": ["tests/loader/builtin/1982_loader_builtin_webp_vp8_alpha_numeric.t"],
            "WBP-05": ["tests/loader/builtin/1981_loader_builtin_webp_lossy_animation_digest.t"],
            "WBP-06": ["tests/loader/builtin/1847_loader_builtin_webp_static_xmp_orientation_and_cms_coexist_code.t"],
        },
    },
    "tga": {
        "title": "Builtin TGA pipeline",
        "subtitle": "Weak probe, indexed fast path, stb raster decode, alpha finalization",
        "document": "docs/loader/builtin/tga.md",
        "stages": [
            stage("TGA-01", "PROBE", "Validate a signature-less TGA header",
                  "src/stb_image.h",
                  ("stbi__tga_test", "stbi__tga_info"),
                  ("check image type, dimensions, depth and palette tuple",
                   "run only after strongly identified formats"), "route"),
            stage("TGA-02", "INDEXED", "Attempt the 8-bit index fast path",
                  "src/loader-builtin.c",
                  ("sixel_builtin_try_load_indexed_tga",),
                  ("call stbi__tga_load_palette when fusion is enabled",
                   "retain PAL8 and normalize binary palette transparency"),
                  "branch"),
            stage("TGA-03", "RASTER", "Decode raw or packet-RLE samples",
                  "src/stb_image.h",
                  ("stbi__tga_load", "stbi__tga_read_rgb16"),
                  ("expand index, gray or BGR(A) source samples",
                   "normalize 5:5:5 and byte component order"), "decode"),
            stage("TGA-04", "ORIGIN", "Normalize the vertical image origin",
                  "src/stb_image.h",
                  ("stbi__tga_load",),
                  ("reverse rows for bottom-origin images",
                   "horizontal right-to-left order remains unsupported"),
                  "parse"),
            stage("TGA-05", "OUTPUT", "Finalize direct or palette alpha",
                  "src/loader-builtin.c",
                  ("sixel_builtin_apply_tga_truecolor_alpha_policy",
                   "sixel_builtin_finalize_loaded_frame"),
                  ("keep 32-bit direct alpha through RGBA8888",
                   "composite or convert supported transparency to a mask"),
                  "output"),
        ],
        "tests": {
            "TGA-01": ["tests/loader/builtin/0010_lsqa_format_tga_type2_rgb.t"],
            "TGA-02": ["tests/loader/builtin/0050_loader_builtin_palette_tga_path.t"],
            "TGA-03": ["tests/loader/builtin/0014_lsqa_format_tga_type10_rgb.t"],
            "TGA-04": ["tests/loader/builtin/0011_lsqa_format_tga_type3_gray.t"],
            "TGA-05": ["tests/loader/builtin/0710_loader_builtin_tga_rgba_background_numeric.t"],
        },
    },
    "pic": {
        "title": "Builtin Softimage PIC pipeline",
        "subtitle": "Fixed header, channel packets, scanline RLE, RGBA output",
        "document": "docs/loader/builtin/pic.md",
        "stages": [
            stage("PIC-01", "PROBE", "Recognize magic plus PICT marker",
                  "src/stb_image.h",
                  ("stbi__pic_test", "stbi__pic_test_core"),
                  ("require 0x5380f634 and PICT at byte 88",
                   "distinguish Softimage PIC from Radiance .pic"), "route"),
            stage("PIC-02", "HEADER", "Read fixed geometry and packet chain",
                  "src/stb_image.h",
                  ("stbi__pic_load", "stbi__pic_load_core"),
                  ("validate width, height and one-to-ten descriptors",
                   "derive selected RGB/A channels and compression modes"),
                  "parse"),
            stage("PIC-03", "RASTER", "Decode raw packet scanlines",
                  "src/stb_image.h",
                  ("stbi__pic_load_core",),
                  ("start from white opaque RGBA",
                   "write selected component tuples for each row"), "decode"),
            stage("PIC-04", "RLE", "Expand pure or mixed RLE packets",
                  "src/stb_image.h",
                  ("stbi__pic_load_core",),
                  ("decode byte and extended repeat counts",
                   "validate mixed-run bounds and input truncation"), "branch"),
            stage("PIC-05", "OUTPUT", "Finalize RGBA and emit one frame",
                  "src/loader-builtin.c",
                  ("sixel_builtin_apply_pic_alpha_policy",
                   "sixel_builtin_finalize_loaded_frame"),
                  ("preserve decoded alpha until common policy",
                   "composite or derive transparency mask from RGBA8888"),
                  "output"),
        ],
        "tests": {
            "PIC-01": ["tests/loader/builtin/0696_loader_builtin_pic_missing_pict_signature_reject.t"],
            "PIC-02": ["tests/loader/builtin/1970_loader_builtin_pic_chained_packets_numeric.t"],
            "PIC-03": ["tests/loader/builtin/1966_loader_builtin_pic_raw_rgb_numeric.t"],
            "PIC-04": ["tests/loader/builtin/1967_loader_builtin_pic_mixed_rle_extended_numeric.t"],
            "PIC-05": ["tests/loader/builtin/0708_loader_builtin_pic_rgba_composite_numeric.t"],
        },
    },
}


ROLE_COLORS = {
    "route": (BLUE, BLUE_LIGHT),
    "parse": (PURPLE, PURPLE_LIGHT),
    "decode": (GOLD, GOLD_LIGHT),
    "branch": (MAGENTA, MAGENTA_LIGHT),
    "color": (PURPLE, PURPLE_LIGHT),
    "output": (TEAL, TEAL_LIGHT),
}


def wrap_symbol_labels(symbols: Sequence[object], limit: int = 86) -> list[str]:
    """Wrap function anchors without truncating their searchable names."""
    lines: list[str] = []
    current = ""
    for symbol in symbols:
        label = str(symbol) + "()"
        candidate = label if not current else current + "  ->  " + label
        if current and len(candidate) > limit:
            lines.append(current)
            current = label
        else:
            current = candidate
    if current:
        lines.append(current)
    return lines


def pipeline_figure(name: str, spec: dict[str, object]) -> str:
    """Render one narrow-screen-friendly vertical implementation map."""
    stages = spec["stages"]
    assert isinstance(stages, list)
    width = 960
    card_height = 184
    card_gap = 46
    first_y = 158
    height = first_y + len(stages) * (card_height + card_gap) + 88
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(42, 58, str(spec["title"]), 36, 820, INK))
    body.append(text(42, 92, str(spec["subtitle"]), 18, 550, MUTED))
    body.append(text(width - 42, 58, "IMPLEMENTATION MAP", 14, 780,
                     BLUE, "end"))

    for index, item in enumerate(stages):
        assert isinstance(item, dict)
        y = first_y + index * (card_height + card_gap)
        role = str(item["role"])
        accent, fill = ROLE_COLORS[role]
        body.append(rect(42, y, 876, card_height, PAPER, GRID, 1.5, 16.0,
                         "card-shadow"))
        body.append(rect(42, y, 9, card_height, accent, radius=4.5))
        body.append(rect(67, y + 22, 105, 30, fill, accent, 1.2, 15.0))
        body.append(text(119.5, y + 43, str(item["coverage_id"]),
                         13, 800, accent, "middle"))
        body.append(text(190, y + 43, str(item["phase"]), 13, 800, MUTED))
        body.append(text(67, y + 79, str(item["title"]), 21, 780, INK))
        symbols = item["symbols"]
        assert isinstance(symbols, list)
        symbol_lines = wrap_symbol_labels(symbols)
        body.append(text_lines(67, y + 107, symbol_lines,
                               12, 18, 650, accent))
        details = item["details"]
        assert isinstance(details, list)
        detail_y = y + 142 + (len(symbol_lines) - 1) * 18
        body.append(text_lines(67, detail_y, [str(value) for value in details],
                               13, 18, 450, MUTED))
        body.append(text(893, y + 43, str(item["source"]), 12, 650,
                         MUTED, "end"))
        if index + 1 < len(stages):
            next_y = y + card_height + card_gap
            body.append(path(
                f"M 480 {y + card_height} V {next_y - 12}",
                accent, 3.5, "arrow-blue"))

    body.append(text(42, height - 39,
                     "Coverage IDs join this diagram to the audit table and owning tests in the format document.",
                     14, 650, MUTED))
    return svg_document(
        width,
        height,
        str(spec["title"]),
        (str(spec["subtitle"]) + ". A vertical sequence of implementation "
         "stages names source modules, function anchors, invariants, and "
         "coverage identifiers that link to the document test audit."),
        "\n".join(body),
    )


def public_manifest() -> dict[str, object]:
    """Return the committed machine-readable pipeline provenance."""
    formats: dict[str, object] = {}
    for name, spec in PIPELINES.items():
        formats[name] = {
            "asset": f"{name}.svg",
            "document": spec["document"],
            "stages": spec["stages"],
            "tests": spec["tests"],
        }
    return {
        "generator": "tools/plot_builtin_loader_format_figures.py",
        "layout": "single responsive vertical SVG per format",
        "formats": formats,
        "color_roles": {
            "routing": BLUE,
            "container_and_policy_parsing": PURPLE,
            "raster_or_entropy_decode": GOLD,
            "conditional_branch": MAGENTA,
            "typed_frame_output": TEAL,
            "coverage_gap_or_error": RED,
        },
    }


def generate(output_dir: Path) -> None:
    """Generate every format SVG and the provenance manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, spec in PIPELINES.items():
        (output_dir / f"{name}.svg").write_text(
            pipeline_figure(name, spec), encoding="utf-8")
    (output_dir / MANIFEST_NAME).write_text(
        json.dumps(public_manifest(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8")


def validate_traceability(source_root: Path) -> list[str]:
    """Return implementation/document/test traceability errors."""
    errors: list[str] = []
    for name, spec in PIPELINES.items():
        document = source_root / str(spec["document"])
        if not document.is_file():
            errors.append(f"{name}: missing document {spec['document']}")
            continue
        document_text = document.read_text(encoding="utf-8")
        asset_ref = f"pipeline-figures/{name}.svg"
        if asset_ref not in document_text:
            errors.append(f"{name}: document does not embed {asset_ref}")
        tests = spec["tests"]
        assert isinstance(tests, dict)
        stages = spec["stages"]
        assert isinstance(stages, list)
        for item in stages:
            assert isinstance(item, dict)
            coverage_id = str(item["coverage_id"])
            if coverage_id not in document_text:
                errors.append(
                    f"{name}: document does not mention {coverage_id}")
            source_path = source_root / str(item["source"])
            if not source_path.is_file():
                errors.append(f"{name}: missing source {item['source']}")
                continue
            source_text = source_path.read_text(encoding="utf-8")
            symbols = item["symbols"]
            assert isinstance(symbols, list)
            for symbol in symbols:
                if str(symbol) not in source_text:
                    errors.append(
                        f"{name}: {symbol} not found in {item['source']}")
            owning_tests = tests.get(coverage_id, [])
            if not owning_tests:
                errors.append(f"{name}: {coverage_id} has no owning test")
            for test_name in owning_tests:
                test_path = source_root / str(test_name)
                if not test_path.is_file():
                    errors.append(f"{name}: missing test {test_name}")
                    continue
                if str(test_name) not in document_text:
                    errors.append(
                        f"{name}: document does not link {test_name}")
                backlink = f"Policy: {spec['document']}"
                if backlink not in test_path.read_text(encoding="utf-8"):
                    errors.append(f"{name}: {test_name} lacks {backlink}")
    return errors


def check(source_root: Path, output_dir: Path) -> None:
    """Compare generated bytes and validate every traceability anchor."""
    expected_names = [f"{name}.svg" for name in PIPELINES]
    expected_names.append(MANIFEST_NAME)
    mismatches: list[str] = []
    with tempfile.TemporaryDirectory(
            prefix="builtin-loader-pipeline-figures-") as temporary:
        generated = Path(temporary)
        generate(generated)
        for name in expected_names:
            expected = output_dir / name
            actual = generated / name
            if (not expected.is_file()
                    or expected.read_bytes() != actual.read_bytes()):
                mismatches.append(name)
    errors = validate_traceability(source_root)
    if mismatches:
        errors.append("stale or missing generated assets: "
                      + ", ".join(mismatches))
    if errors:
        raise SystemExit("\n".join(errors))


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir", type=Path, default=source_root / OUTPUT_DIR,
        help="directory for generated SVG and manifest files")
    parser.add_argument(
        "--check", action="store_true",
        help="verify generated assets and implementation/test traceability")
    return parser.parse_args()


def main() -> None:
    """Generate or verify builtin-loader pipeline assets."""
    arguments = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    if arguments.check:
        check(source_root, arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
