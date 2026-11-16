/* SPDX-License-Identifier: MIT */
#ifndef ICC_NUMERIC_COMMON_H
#define ICC_NUMERIC_COMMON_H
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
#endif
