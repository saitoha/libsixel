/* SPDX-License-Identifier: MIT */
/* Compile the actual private scan so optional-path selection is observable. */
#include <math.h>
#if !defined(SIXEL_AMALGAMATION)
#define sixel_lookup_policy_5bit_8bit_new filtered_5_byte_new
#define sixel_lookup_policy_5bit_float32_new filtered_5_float_new
#include "src/lookup-policy-5bit.c"
#undef sixel_lookup_policy_5bit_8bit_new
#undef sixel_lookup_policy_5bit_float32_new
#define FILTER_NEW filtered_5_float_new
#else
#define FILTER_NEW sixel_lookup_policy_5bit_float32_new
#endif
#define FILTER_OBJECT sixel_lookup_policy_bit5_object_t
#define FILTER_TEST test_filtered_5bit
#define filtered_oracle filtered_oracle_5
#define filtered_malloc filtered_malloc_5
#define filtered_allocations filtered_allocations_5
#define filtered_fail_at filtered_fail_at_5
#include "tests/_support/processing/lookup/filtered/filtered_common.inc.c"
#undef filtered_oracle
#undef filtered_malloc
#undef filtered_allocations
#undef filtered_fail_at
#undef FILTER_NEW
#undef FILTER_OBJECT
#undef FILTER_TEST


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
