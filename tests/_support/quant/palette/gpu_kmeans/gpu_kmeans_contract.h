/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_TEST_KMEANS_GPU_CONTRACT_H
#define SIXEL_TEST_KMEANS_GPU_CONTRACT_H
int kmgpu_contract_ties(int mode);
int kmgpu_contract_numeric(int mode);
int kmgpu_contract_degenerate(int mode);
int kmgpu_contract_fault(int stage, int force, int mixed);
int kmgpu_contract_workload(void);
int kmgpu_contract_auto(int mode);
int kmgpu_contract_image(char const *, unsigned int, unsigned int,
    unsigned int, int);
int kmgpu_contract_numeric_fallback(int force);
#endif
