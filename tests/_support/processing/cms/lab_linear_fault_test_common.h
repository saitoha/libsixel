/* SPDX-License-Identifier: MIT */
#ifndef TEST_LAB_LINEAR_FAULT_COMMON_H
#define TEST_LAB_LINEAR_FAULT_COMMON_H
/* These modes exist only in the separately compiled test source. */
enum {
    CMS_LAB_FAIL_BUILTIN_LATE = 1,
    CMS_LAB_FAIL_COLORSYNC_LATE,
    CMS_LAB_FAIL_ALLOCATION,
    CMS_LAB_FAIL_NAN,
    CMS_LAB_FAIL_INFINITY
};
int cms_lab_failure_probe(int mode);
int tiff_lab_failure_probe(int argc, char **argv, int mode);
#endif
