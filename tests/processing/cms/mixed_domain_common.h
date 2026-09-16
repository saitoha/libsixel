/* SPDX-License-Identifier: MIT */
#ifndef TEST_CMS_MIXED_DOMAIN_COMMON_H
#define TEST_CMS_MIXED_DOMAIN_COMMON_H
#include <stddef.h>
#define CMS_MIXED_MAX 28u
typedef struct cms_mixed_fixture {
    unsigned char const *icc;
    size_t length;
    size_t normalized;
    size_t count;
    int gray;
    int swap;
    int non_target;
    float input[CMS_MIXED_MAX * 3u];
    double ideal[CMS_MIXED_MAX * 3u];
} cms_mixed_fixture_t;
void cms_mixed_fixture(int kind, cms_mixed_fixture_t *fixture);
int cms_mixed_values(int observation);
int cms_mixed_failure(int observation);
#endif
