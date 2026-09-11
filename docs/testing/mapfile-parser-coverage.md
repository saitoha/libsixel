# Mapfile Parser Coverage Inventory

## Purpose

This inventory keeps every test in `tests/quant/mapfile/` discoverable without treating every malformed-input variant or smoke test as a separate public guarantee. The user-visible format and option contracts remain owned by [External Palette Input and Output](../functionality/external-palettes.md). Tests classified as behavioral contracts carry both a `Policy:` backlink to that document and a `Test-plan:` backlink here. Defensive and supplementary tests carry only `Test-plan:`.

The observation text below mirrors each test's opening comment. The test itself remains authoritative for fixture construction, exact assertions, skips, and failure localization.

## Complete inventory

<!-- test-plan: tests/quant/mapfile/*.t -->

| Test | Observation | Traceability role |
| --- | --- | --- |
| [tests/quant/mapfile/0001_mapfile_export_act.t](../../tests/quant/mapfile/0001_mapfile_export_act.t) | ACT palette export writes expected size. | Supplementary acceptance or smoke |
| [tests/quant/mapfile/0002_mapfile_export_pal_default.t](../../tests/quant/mapfile/0002_mapfile_export_pal_default.t) | default PAL palette export writes JASC-PAL header. | Supplementary acceptance or smoke |
| [tests/quant/mapfile/0003_mapfile_export_pal_stdout.t](../../tests/quant/mapfile/0003_mapfile_export_pal_stdout.t) | PAL export to stdout retains JASC-PAL header. | Supplementary acceptance or smoke |
| [tests/quant/mapfile/0004_mapfile_export_riff.t](../../tests/quant/mapfile/0004_mapfile_export_riff.t) | RIFF palette export writes RIFF header bytes. | Supplementary acceptance or smoke |
| [tests/quant/mapfile/0005_mapfile_export_gpl.t](../../tests/quant/mapfile/0005_mapfile_export_gpl.t) | GPL palette export writes GIMP header. | Supplementary acceptance or smoke |
| [tests/quant/mapfile/0006_mapfile_import_gpl_prefixed.t](../../tests/quant/mapfile/0006_mapfile_import_gpl_prefixed.t) | GPL palette input via type prefix converts image data. | Behavioral contract |
| [tests/quant/mapfile/0007_mapfile_import_act_extension.t](../../tests/quant/mapfile/0007_mapfile_import_act_extension.t) | ACT palette import by extension converts image data. | Behavioral contract |
| [tests/quant/mapfile/0008_mapfile_import_riff_explicit.t](../../tests/quant/mapfile/0008_mapfile_import_riff_explicit.t) | RIFF palette import by explicit type converts image data. | Behavioral contract |
| [tests/quant/mapfile/0009_mapfile_import_stdin.t](../../tests/quant/mapfile/0009_mapfile_import_stdin.t) | GPL palette import from stdin converts image data. | Behavioral contract |
| [tests/quant/mapfile/0010_mapfile_import_act_rejects_count_over_256.t](../../tests/quant/mapfile/0010_mapfile_import_act_rejects_count_over_256.t) | ACT import rejects trailer color counts above 256. | Behavioral contract |
| [tests/quant/mapfile/0011_mapfile_import_pal_rejects_count_suffix.t](../../tests/quant/mapfile/0011_mapfile_import_pal_rejects_count_suffix.t) | JASC-PAL import rejects color counts with trailing garbage. | Behavioral contract |
| [tests/quant/mapfile/0012_mapfile_import_pal_rejects_component_suffix.t](../../tests/quant/mapfile/0012_mapfile_import_pal_rejects_component_suffix.t) | JASC-PAL import rejects component tokens with trailing garbage. | Behavioral contract |
| [tests/quant/mapfile/0013_mapfile_import_gpl_rejects_component_suffix.t](../../tests/quant/mapfile/0013_mapfile_import_gpl_rejects_component_suffix.t) | GPL import rejects component tokens with trailing garbage. | Behavioral contract |
| [tests/quant/mapfile/0014_mapfile_import_rejects_input_over_16mib.t](../../tests/quant/mapfile/0014_mapfile_import_rejects_input_over_16mib.t) | mapfile import rejects palette streams larger than 16 MiB. | Behavioral contract |
| [tests/quant/mapfile/0015_mapfile_import_accepts_exactly_16mib.t](../../tests/quant/mapfile/0015_mapfile_import_accepts_exactly_16mib.t) | mapfile import accepts palette streams exactly 16 MiB long. | Behavioral contract |
| [tests/quant/mapfile/0016_mapfile_import_act_accepts_zero_color_count.t](../../tests/quant/mapfile/0016_mapfile_import_act_accepts_zero_color_count.t) | ACT import keeps count=0 compatibility by treating it as 256. | Behavioral contract |
| [tests/quant/mapfile/0017_mapfile_import_riff_rejects_chunk_payload_overrun.t](../../tests/quant/mapfile/0017_mapfile_import_riff_rejects_chunk_payload_overrun.t) | RIFF import rejects chunks whose payload size overruns input. | Behavioral contract |
| [tests/quant/mapfile/0018_mapfile_import_riff_rejects_missing_chunk_padding.t](../../tests/quant/mapfile/0018_mapfile_import_riff_rejects_missing_chunk_padding.t) | RIFF import rejects odd-sized chunks without pad byte. | Behavioral contract |
| [tests/quant/mapfile/0019_mapfile_import_gpl_accepts_label_suffix.t](../../tests/quant/mapfile/0019_mapfile_import_gpl_accepts_label_suffix.t) | GPL import keeps compatibility with trailing color labels. | Behavioral contract |
| [tests/quant/mapfile/0020_mapfile_import_pal_accepts_version_0100.t](../../tests/quant/mapfile/0020_mapfile_import_pal_accepts_version_0100.t) | JASC-PAL import accepts version line 0100. | Behavioral contract |
| [tests/quant/mapfile/0021_mapfile_import_pal_rejects_version_0099.t](../../tests/quant/mapfile/0021_mapfile_import_pal_rejects_version_0099.t) | JASC-PAL import rejects version line 0099. | Behavioral contract |
| [tests/quant/mapfile/0022_mapfile_import_pal_rejects_version_0200.t](../../tests/quant/mapfile/0022_mapfile_import_pal_rejects_version_0200.t) | JASC-PAL import rejects version line 0200. | Behavioral contract |
| [tests/quant/mapfile/0023_mapfile_import_pal_rejects_version_suffix.t](../../tests/quant/mapfile/0023_mapfile_import_pal_rejects_version_suffix.t) | JASC-PAL import rejects version line with non-digit suffix. | Behavioral contract |
| [tests/quant/mapfile/0024_mapfile_import_act_accepts_transparency_255_count_1.t](../../tests/quant/mapfile/0024_mapfile_import_act_accepts_transparency_255_count_1.t) | ACT import accepts transparency index 255 and count 1. | Behavioral contract |
| [tests/quant/mapfile/0025_mapfile_import_act_accepts_transparency_255_count_2.t](../../tests/quant/mapfile/0025_mapfile_import_act_accepts_transparency_255_count_2.t) | ACT import accepts transparency index 255 and count 2. | Behavioral contract |
| [tests/quant/mapfile/0026_mapfile_import_act_accepts_transparency_0_count_256.t](../../tests/quant/mapfile/0026_mapfile_import_act_accepts_transparency_0_count_256.t) | ACT import accepts transparency index 0 and count 256. | Behavioral contract |
| [tests/quant/mapfile/0027_mapfile_import_riff_rejects_declared_size_mismatch.t](../../tests/quant/mapfile/0027_mapfile_import_riff_rejects_declared_size_mismatch.t) | RIFF import rejects mismatched RIFF size fields. | Behavioral contract |
| [tests/quant/mapfile/0028_mapfile_import_riff_rejects_missing_data_chunk.t](../../tests/quant/mapfile/0028_mapfile_import_riff_rejects_missing_data_chunk.t) | RIFF import rejects palettes without a data chunk. | Behavioral contract |
| [tests/quant/mapfile/0029_mapfile_import_riff_rejects_entry_count_zero.t](../../tests/quant/mapfile/0029_mapfile_import_riff_rejects_entry_count_zero.t) | RIFF import rejects data chunks with entry_count=0. | Behavioral contract |
| [tests/quant/mapfile/0030_mapfile_import_riff_rejects_entry_count_over_256.t](../../tests/quant/mapfile/0030_mapfile_import_riff_rejects_entry_count_over_256.t) | RIFF import rejects data chunks with entry_count=257. | Behavioral contract |
| [tests/quant/mapfile/0031_mapfile_import_riff_rejects_duplicate_data_chunk.t](../../tests/quant/mapfile/0031_mapfile_import_riff_rejects_duplicate_data_chunk.t) | RIFF import rejects palettes containing duplicate data chunks. | Behavioral contract |
| [tests/quant/mapfile/0032_mapfile_import_pal_rejects_embedded_nul.t](../../tests/quant/mapfile/0032_mapfile_import_pal_rejects_embedded_nul.t) | JASC-PAL import rejects embedded NUL bytes. | Behavioral contract |
| [tests/quant/mapfile/0033_mapfile_import_gpl_rejects_embedded_nul.t](../../tests/quant/mapfile/0033_mapfile_import_gpl_rejects_embedded_nul.t) | GPL import rejects embedded NUL bytes. | Behavioral contract |
| [tests/quant/mapfile/0034_mapfile_import_rejects_extensionless_size_only_act.t](../../tests/quant/mapfile/0034_mapfile_import_rejects_extensionless_size_only_act.t) | extensionless ACT-sized data is rejected as ambiguous. | Behavioral contract |
| [tests/quant/mapfile/0035_mapfile_import_accepts_extensionless_act_with_prefix.t](../../tests/quant/mapfile/0035_mapfile_import_accepts_extensionless_act_with_prefix.t) | extensionless ACT-sized data works with an explicit act: prefix. | Behavioral contract |
| [tests/quant/mapfile/0036_mapfile_import_prefers_jasc_signature_over_act_size.t](../../tests/quant/mapfile/0036_mapfile_import_prefers_jasc_signature_over_act_size.t) | extensionless JASC signature is preferred over ACT size heuristics. | Behavioral contract |
| [tests/quant/mapfile/0037_mapfile_import_riff_rejects_invalid_version.t](../../tests/quant/mapfile/0037_mapfile_import_riff_rejects_invalid_version.t) | RIFF import rejects unsupported palette version values. | Behavioral contract |
| [tests/quant/mapfile/0038_mapfile_import_riff_rejects_trailing_bytes.t](../../tests/quant/mapfile/0038_mapfile_import_riff_rejects_trailing_bytes.t) | RIFF import rejects trailing bytes after chunk traversal. | Behavioral contract |
| [tests/quant/mapfile/0039_mapfile_import_pal_rejects_non_utf8_bom.t](../../tests/quant/mapfile/0039_mapfile_import_pal_rejects_non_utf8_bom.t) | JASC-PAL import rejects UTF-16/32 BOM encoded input. | Behavioral contract |
| [tests/quant/mapfile/0040_mapfile_import_gpl_rejects_non_utf8_bom.t](../../tests/quant/mapfile/0040_mapfile_import_gpl_rejects_non_utf8_bom.t) | GPL import rejects UTF-16/32 BOM encoded input. | Behavioral contract |
| [tests/quant/mapfile/0041_mapfile_import_gpl_rejects_extra_numeric_token.t](../../tests/quant/mapfile/0041_mapfile_import_gpl_rejects_extra_numeric_token.t) | GPL import rejects a fourth numeric token after RGB triplet. | Behavioral contract |
| [tests/quant/mapfile/0042_mapfile_import_pal_accepts_utf8_bom.t](../../tests/quant/mapfile/0042_mapfile_import_pal_accepts_utf8_bom.t) | JASC-PAL import accepts UTF-8 BOM. | Behavioral contract |
| [tests/quant/mapfile/0043_mapfile_import_gpl_accepts_utf8_bom.t](../../tests/quant/mapfile/0043_mapfile_import_gpl_accepts_utf8_bom.t) | GPL import accepts UTF-8 BOM. | Behavioral contract |
| [tests/quant/mapfile/0044_mapfile_import_rejects_unknown_extensionless_format.t](../../tests/quant/mapfile/0044_mapfile_import_rejects_unknown_extensionless_format.t) | extensionless unknown palette payload is rejected. | Behavioral contract |
| [tests/quant/mapfile/0045_mapfile_import_act_transparency_does_not_offset_palette.t](../../tests/quant/mapfile/0045_mapfile_import_act_transparency_does_not_offset_palette.t) | Verify ACT transparency metadata does not offset imported palette entries. | Behavioral contract |
| [tests/quant/mapfile/0046_mapfile_import_rejects_pal_extension_with_act_payload.t](../../tests/quant/mapfile/0046_mapfile_import_rejects_pal_extension_with_act_payload.t) | .pal input rejects ACT-sized binary payload ambiguity. | Behavioral contract |
| [tests/quant/mapfile/0047_mapfile_import_accepts_pal_jasc_prefix_over_extension.t](../../tests/quant/mapfile/0047_mapfile_import_accepts_pal_jasc_prefix_over_extension.t) | pal-jasc prefix overrides mismatched .act extension. | Behavioral contract |
| [tests/quant/mapfile/0048_mapfile_import_stdin_rejects_input_over_16mib.t](../../tests/quant/mapfile/0048_mapfile_import_stdin_rejects_input_over_16mib.t) | stdin mapfile import rejects payloads above 16 MiB. | Behavioral contract |
| [tests/quant/mapfile/0049_mapfile_import_stdin_accepts_exactly_16mib.t](../../tests/quant/mapfile/0049_mapfile_import_stdin_accepts_exactly_16mib.t) | stdin mapfile import accepts payloads at exactly 16 MiB. | Behavioral contract |
| [tests/quant/mapfile/0050_mapfile_import_accepts_uppercase_prefix.t](../../tests/quant/mapfile/0050_mapfile_import_accepts_uppercase_prefix.t) | mapfile format prefixes are accepted case-insensitively. | Behavioral contract |
| [tests/quant/mapfile/0051_mapfile_import_accepts_extensionless_riff_signature.t](../../tests/quant/mapfile/0051_mapfile_import_accepts_extensionless_riff_signature.t) | extensionless RIFF palette is accepted by signature detection. | Behavioral contract |
| [tests/quant/mapfile/0052_mapfile_import_accepts_pal_riff_prefix_over_extension.t](../../tests/quant/mapfile/0052_mapfile_import_accepts_pal_riff_prefix_over_extension.t) | pal-riff prefix overrides mismatched .gpl extension. | Behavioral contract |
| [tests/quant/mapfile/0053_mapfile_import_rejects_missing_path.t](../../tests/quant/mapfile/0053_mapfile_import_rejects_missing_path.t) | missing mapfile paths are rejected. | Behavioral contract |
| [tests/quant/mapfile/0054_mapfile_import_rejects_directory_path.t](../../tests/quant/mapfile/0054_mapfile_import_rejects_directory_path.t) | directory mapfile paths are rejected. | Behavioral contract |
| [tests/quant/mapfile/0055_mapfile_import_rejects_empty_option.t](../../tests/quant/mapfile/0055_mapfile_import_rejects_empty_option.t) | empty mapfile option values are rejected. | Behavioral contract |
| [tests/quant/mapfile/0056_mapfile_import_rejects_prefix_only.t](../../tests/quant/mapfile/0056_mapfile_import_rejects_prefix_only.t) | prefix-only mapfile paths are rejected. | Behavioral contract |
| [tests/quant/mapfile/0057_mapfile_import_rejects_empty_stdin.t](../../tests/quant/mapfile/0057_mapfile_import_rejects_empty_stdin.t) | empty stdin mapfiles are rejected. | Behavioral contract |
| [tests/quant/mapfile/0058_mapfile_import_act_rejects_truncated_palette.t](../../tests/quant/mapfile/0058_mapfile_import_act_rejects_truncated_palette.t) | ACT import rejects truncated payloads. | Behavioral contract |
| [tests/quant/mapfile/0059_mapfile_import_act_rejects_invalid_length.t](../../tests/quant/mapfile/0059_mapfile_import_act_rejects_invalid_length.t) | ACT import rejects invalid non-record-aligned lengths. | Behavioral contract |
| [tests/quant/mapfile/0060_mapfile_import_pal_rejects_missing_header.t](../../tests/quant/mapfile/0060_mapfile_import_pal_rejects_missing_header.t) | JASC-PAL import rejects missing headers. | Behavioral contract |
| [tests/quant/mapfile/0061_mapfile_import_pal_rejects_incomplete_header.t](../../tests/quant/mapfile/0061_mapfile_import_pal_rejects_incomplete_header.t) | JASC-PAL import rejects incomplete headers. | Behavioral contract |
| [tests/quant/mapfile/0062_mapfile_import_pal_rejects_count_zero.t](../../tests/quant/mapfile/0062_mapfile_import_pal_rejects_count_zero.t) | JASC-PAL import rejects zero color counts. | Behavioral contract |
| [tests/quant/mapfile/0063_mapfile_import_pal_rejects_count_over_256.t](../../tests/quant/mapfile/0063_mapfile_import_pal_rejects_count_over_256.t) | JASC-PAL import rejects color counts over 256. | Behavioral contract |
| [tests/quant/mapfile/0064_mapfile_import_pal_rejects_missing_component.t](../../tests/quant/mapfile/0064_mapfile_import_pal_rejects_missing_component.t) | JASC-PAL import rejects missing RGB components. | Behavioral contract |
| [tests/quant/mapfile/0065_mapfile_import_pal_rejects_negative_component.t](../../tests/quant/mapfile/0065_mapfile_import_pal_rejects_negative_component.t) | JASC-PAL import rejects negative RGB components. | Behavioral contract |
| [tests/quant/mapfile/0066_mapfile_import_pal_rejects_component_over_255.t](../../tests/quant/mapfile/0066_mapfile_import_pal_rejects_component_over_255.t) | JASC-PAL import rejects RGB components over 255. | Behavioral contract |
| [tests/quant/mapfile/0067_mapfile_import_pal_rejects_excess_entries.t](../../tests/quant/mapfile/0067_mapfile_import_pal_rejects_excess_entries.t) | JASC-PAL import rejects entries beyond the declared count. | Behavioral contract |
| [tests/quant/mapfile/0068_mapfile_import_pal_rejects_color_count_mismatch.t](../../tests/quant/mapfile/0068_mapfile_import_pal_rejects_color_count_mismatch.t) | JASC-PAL import rejects short color tables. | Behavioral contract |
| [tests/quant/mapfile/0069_mapfile_import_riff_rejects_truncated_palette.t](../../tests/quant/mapfile/0069_mapfile_import_riff_rejects_truncated_palette.t) | RIFF PAL import rejects truncated payloads. | Behavioral contract |
| [tests/quant/mapfile/0070_mapfile_import_riff_rejects_missing_header.t](../../tests/quant/mapfile/0070_mapfile_import_riff_rejects_missing_header.t) | RIFF PAL import rejects missing RIFF headers. | Behavioral contract |
| [tests/quant/mapfile/0071_mapfile_import_riff_rejects_data_chunk_too_small.t](../../tests/quant/mapfile/0071_mapfile_import_riff_rejects_data_chunk_too_small.t) | RIFF PAL import rejects undersized data chunks. | Behavioral contract |
| [tests/quant/mapfile/0072_mapfile_import_riff_rejects_unexpected_chunk_size.t](../../tests/quant/mapfile/0072_mapfile_import_riff_rejects_unexpected_chunk_size.t) | RIFF PAL import rejects data chunks whose size disagrees with entry count. | Behavioral contract |
| [tests/quant/mapfile/0073_mapfile_import_gpl_rejects_missing_header.t](../../tests/quant/mapfile/0073_mapfile_import_gpl_rejects_missing_header.t) | GPL import rejects first payload lines without a header. | Behavioral contract |
| [tests/quant/mapfile/0074_mapfile_import_gpl_rejects_metadata_only_header_missing.t](../../tests/quant/mapfile/0074_mapfile_import_gpl_rejects_metadata_only_header_missing.t) | GPL import rejects metadata-only payloads without a header. | Behavioral contract |
| [tests/quant/mapfile/0075_mapfile_import_gpl_rejects_no_colors.t](../../tests/quant/mapfile/0075_mapfile_import_gpl_rejects_no_colors.t) | GPL import rejects headers without colors. | Behavioral contract |
| [tests/quant/mapfile/0076_mapfile_import_gpl_rejects_too_many_colors.t](../../tests/quant/mapfile/0076_mapfile_import_gpl_rejects_too_many_colors.t) | GPL import rejects more than 256 colors. | Behavioral contract |
| [tests/quant/mapfile/0077_mapfile_import_gpl_rejects_missing_component.t](../../tests/quant/mapfile/0077_mapfile_import_gpl_rejects_missing_component.t) | GPL import rejects missing RGB components. | Behavioral contract |
| [tests/quant/mapfile/0078_mapfile_import_gpl_rejects_negative_component.t](../../tests/quant/mapfile/0078_mapfile_import_gpl_rejects_negative_component.t) | GPL import rejects negative RGB components. | Behavioral contract |
| [tests/quant/mapfile/0079_mapfile_import_gpl_rejects_component_over_255.t](../../tests/quant/mapfile/0079_mapfile_import_gpl_rejects_component_over_255.t) | GPL import rejects RGB components over 255. | Behavioral contract |
| [tests/quant/mapfile/0080_mapfile_export_act_exact_layout.t](../../tests/quant/mapfile/0080_mapfile_export_act_exact_layout.t) | Verify ACT export writes the complete canonical 772-byte layout. | Behavioral contract |
| [tests/quant/mapfile/0081_mapfile_import_act_768_exact.t](../../tests/quant/mapfile/0081_mapfile_import_act_768_exact.t) | Verify a trailer-free 768-byte ACT imports all 256 ordered entries. | Behavioral contract |
| [tests/quant/mapfile/0082_mapfile_export_jasc_exact_layout.t](../../tests/quant/mapfile/0082_mapfile_export_jasc_exact_layout.t) | Verify JASC PAL export writes the complete canonical text layout. | Behavioral contract |
| [tests/quant/mapfile/0083_mapfile_import_jasc_lf.t](../../tests/quant/mapfile/0083_mapfile_import_jasc_lf.t) | Verify JASC PAL import accepts LF line endings. | Behavioral contract |
| [tests/quant/mapfile/0084_mapfile_import_jasc_cr.t](../../tests/quant/mapfile/0084_mapfile_import_jasc_cr.t) | Verify JASC PAL import accepts CR line endings. | Behavioral contract |
| [tests/quant/mapfile/0085_mapfile_import_jasc_crlf.t](../../tests/quant/mapfile/0085_mapfile_import_jasc_crlf.t) | Verify JASC PAL import accepts CRLF line endings. | Behavioral contract |
| [tests/quant/mapfile/0086_mapfile_export_riff_exact_layout.t](../../tests/quant/mapfile/0086_mapfile_export_riff_exact_layout.t) | Verify RIFF PAL export writes the complete canonical binary layout. | Behavioral contract |
| [tests/quant/mapfile/0087_mapfile_import_riff_exact_entries.t](../../tests/quant/mapfile/0087_mapfile_import_riff_exact_entries.t) | Verify an independent RIFF PAL imports ordered RGB and ignores entry flags. | Behavioral contract |
| [tests/quant/mapfile/0088_mapfile_import_gpl_exact_entries.t](../../tests/quant/mapfile/0088_mapfile_import_gpl_exact_entries.t) | Verify an independent GPL imports RGB while discarding text metadata. | Behavioral contract |
| [tests/quant/mapfile/0089_mapfile_import_gpl_lf.t](../../tests/quant/mapfile/0089_mapfile_import_gpl_lf.t) | Verify GPL import accepts LF line endings. | Behavioral contract |
| [tests/quant/mapfile/0090_mapfile_import_gpl_cr.t](../../tests/quant/mapfile/0090_mapfile_import_gpl_cr.t) | Verify GPL import accepts CR line endings. | Behavioral contract |
| [tests/quant/mapfile/0091_mapfile_import_gpl_crlf.t](../../tests/quant/mapfile/0091_mapfile_import_gpl_crlf.t) | Verify GPL import accepts CRLF line endings. | Behavioral contract |

<!-- test-plan-end -->

## Maintenance

The marker glob, every link in the delimited inventory, and every reciprocal `Test-plan:` source comment are compared by [`staticcheck-test-plan-links`](../../tests/_static/sh/staticcheck-test-plan-links.sh). Additions, deletions, and renames must update the test and this inventory in the same change. Promote a test to the behavioral coverage table in the owning policy document only when it directly proves a deliberately stable public contract.
