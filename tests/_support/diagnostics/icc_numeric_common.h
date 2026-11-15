/* SPDX-License-Identifier: MIT */
#ifndef ICC_NUMERIC_COMMON_H
#define ICC_NUMERIC_COMMON_H
int icc_numeric_parse_test(int segmented, int failure);
int icc_numeric_loader_test(int segmented);
int icc_numeric_reverse_matrix_test(int lut16, char const *tag_name);
int icc_numeric_absolute_matrix_test(void);
int icc_numeric_absolute_lut_test(int fallback);
int icc_numeric_absolute_definition_test(void);
int icc_numeric_profile_status_test(int malformed, int keep_matrix);
int icc_numeric_native_fallback_status_test(void);
int icc_numeric_apply_status_test(void);
int icc_numeric_tag_budget_test(int over_limit);
int icc_numeric_parse_budget_test(int over_limit);
#endif
