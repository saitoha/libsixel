/* SPDX-License-Identifier: MIT */
#ifndef TEST_RETAINED_WORKERS_COMMON_H
#define TEST_RETAINED_WORKERS_COMMON_H

enum retained_workers_observation {
    RETAINED_WORKERS_RGB,
    RETAINED_WORKERS_VALID,
    RETAINED_WORKERS_PAINT,
    RETAINED_WORKERS_COUNT,
    RETAINED_WORKERS_REFINEMENT,
    RETAINED_WORKERS_POLICY,
    RETAINED_WORKERS_BALANCED,
    RETAINED_WORKERS_NO_ALLOCATION,
    RETAINED_WORKERS_REUSE_SMALLER
};

int retained_workers_check(int participants,
    enum retained_workers_observation observation);

#endif
