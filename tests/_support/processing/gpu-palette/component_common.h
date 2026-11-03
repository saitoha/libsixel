/* SPDX-License-Identifier: MIT */
#ifndef TEST_GPU_COMPONENT_COMMON_H
#define TEST_GPU_COMPONENT_COMMON_H
#include "src/gpu-lookup-ordered.h"
#include "src/gpu-lookup-vptree.h"
#include "src/gpu-lookup-rbc.h"
#include "src/gpu-lookup-mahalanobis.h"
#include "src/palette.h"

typedef struct gpu_component_fixture {
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    sixel_gpu_lookup_preparer_interface_t *provider;
    sixel_gpu_lookup_plan_interface_t *plan;
    sixel_gpu_lookup_plan_interface_t *other;
    sixel_gpu_lookup_binding_interface_t *binding;
    sixel_gpu_lookup_binding_interface_t *second;
    sixel_lookup_execution_request_t request;
    sixel_gpu_device_interface_t device;
    sixel_gpu_device_interface_t other_device;
} gpu_component_fixture_t;

int gpu_component_open(gpu_component_fixture_t *, int);
void gpu_component_close(gpu_component_fixture_t *);
int gpu_component_set_palette(gpu_component_fixture_t *, unsigned char);
#endif
