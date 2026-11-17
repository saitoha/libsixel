/* SPDX-License-Identifier: MIT */
#ifndef ICC_NUMERIC_COMMON_H
#define ICC_NUMERIC_COMMON_H
#include <stddef.h>
int icc_numeric_parse_test(int segmented, int failure);
int icc_numeric_loader_test(int segmented);
int icc_numeric_reverse_matrix_test(int lut16, char const *tag_name);
int icc_numeric_absolute_matrix_test(void);
int icc_numeric_absolute_lut_test(int fallback);
int icc_numeric_absolute_equal_white_test(void);
int icc_numeric_absolute_after_missing_relative_test(void);
int icc_numeric_absolute_unpaired_test(void);
int icc_numeric_absolute_definition_test(void);
int icc_numeric_profile_status_test(int malformed, int keep_matrix);
int icc_numeric_native_fallback_status_test(void);
int icc_numeric_apply_status_test(void);
int icc_numeric_tag_budget_test(int over_limit);
int icc_numeric_parse_budget_test(int over_limit);
int icc_numeric_mpet_order_test(void);
int icc_numeric_mpet_position_test(void);
int icc_numeric_mpet_channels_test(void);
int icc_numeric_mpet_unknown_test(void);
int icc_numeric_mpet_no_path_test(void);
int icc_numeric_mpet_count_test(int over_limit);
int icc_numeric_mpet_kinds_test(void);
int icc_mpet_curve_formulas_test(void);
int icc_mpet_curve_samples_test(void);
int icc_mpet_curve_structure_test(void);
int icc_mpet_curve_unsupported_test(void);
int icc_mpet_curve_evaluation_failure_test(void);
int icc_mpet_curve_log_range_test(void);
int icc_mpet_curve_sample_endpoint_test(void);
int icc_numeric_mpet_slot3_status_test(void);
int icc_mpet_float_matrix_test(void);
int icc_mpet_float_clut_test(void);
int icc_mpet_numeric_payload_test(void);
int icc_mpet_chain_test(int reverse);
int icc_mpet_absolute_test(int relative_endpoint);
int icc_mpet_float_pcs_lab_test(int reverse);
int icc_mpet_missing_intent_test(void);

int icc_numeric_profile_header_test(int specimen);
int icc_numeric_profile_missing_header_test(void);
int icc_numeric_colorspace_lab_fallback_test(int legacy);

int icc_numeric_mpet_runtime_fallback_test(void);
int icc_numeric_mpet_clut_budget_test(void);
int icc_numeric_mpet_bmp_test(int absolute, int reject);

int icc_numeric_mpet_legacy_color_test(void);
int icc_numeric_mpet_fallback_from_absolute_test(void);
int icc_numeric_mpet_fallback_to_absolute_test(void);
int icc_numeric_mpet_matrix_target_intent_test(void);
int icc_numeric_mpet_raw_lab_target_test(void);
int icc_numeric_colorspace_lab_slot_zero_test(int reverse);
int icc_numeric_mpet_high_dimensional_clut_test(void);
int icc_numeric_mpet_legacy_slot_pair_test(void);
int icc_numeric_mpet_lab_neutral_test(int reverse);
int icc_numeric_mpet_reachable_legacy_pair_test(void);
int icc_numeric_colorspace_legacy_rgb8_test(void);
int icc_numeric_mpet_definition_absolute_fallback_test(int reverse);
size_t icc_numeric_rgba_legacy_profile(unsigned char bytes[516]);
int icc_numeric_mpet_simplex_zero_weight_test(void);
int icc_numeric_mpet_lab_pcs_neutral_test(int reverse);
int icc_numeric_colorspace_legacy_gray8_test(void);
int icc_numeric_mpet_legacy_reverse_precedence_test(void);
int icc_numeric_owned_mpet_matrix_fallback_test(int reverse);
int icc_numeric_mpet_float_storage_fallback_test(void);
int icc_numeric_mpet_simplex_cancellation_test(int amplify);
size_t icc_numeric_gray_legacy_profile(unsigned char bytes[260]);
int icc_numeric_colorspace_legacy_float_pair_test(int byte_input);
int icc_numeric_bpc_relative_before_cached_pair_test(int byte_input);
int icc_numeric_mpet_before_later_legacy_test(void);
int icc_numeric_mpet_legacy_mixed_precedence_test(int byte_input,
                                                int saturation_first);
int icc_numeric_mpet_tag_share_budget_test(int distinct);
int icc_numeric_mpet_downstream_fallback_test(int absolute);
int icc_numeric_mpet_small_residual_test(unsigned int clut_axes);
int icc_numeric_mpet_matrix_product_residual_test(void);
int icc_numeric_mpet_curve_small_residual_test(void);
int icc_numeric_mpet_palette_test(int reverse, char slot, int gamut);
int icc_numeric_nonrelative_mpet_palette_test(int gamut);
int icc_numeric_mpet_png_storage_fallback_test(void);

#endif
