/* SPDX-License-Identifier: MIT */
#ifndef TEST_RETAINED_COMPONENT_COMMON_H
#define TEST_RETAINED_COMPONENT_COMMON_H

enum retained_component_observation {
    RETAINED_COMPONENT_WORKERS,
    RETAINED_COMPONENT_SHRINK,
    RETAINED_COMPONENT_GROW,
    RETAINED_COMPONENT_ADMISSION,
    RETAINED_COMPONENT_NO_ALLOCATION,
    RETAINED_COMPONENT_NEGATIVE,
    RETAINED_COMPONENT_ABORT,
    RETAINED_COMPONENT_WRONG_RECEIPT
};

int retained_component_check(int large, int budget, int expected,
    enum retained_component_observation observation);

#endif
