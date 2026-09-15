/* SPDX-License-Identifier: MIT */
/* Exercise the actual loader manager and PNG decoder with an unavailable
 * arithmetic backend. Only factory lookup and the selected core build fail.
 * The implementation is included, not copied or replaced with decode stubs.
 * Keep this namespace list aligned with new file-local production symbols;
 * the amalgamated test build checks that both copies remain independent. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include "src/factory.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "src/loader-common.h"
#include "src/options.h"
#include "src/png-chrm-arithmetic.h"
#include "src/chunk-factory.h"
#include "tests/data/color-numeric/png-arithmetic.h"

#if HAVE_LIBPNG
static sixel_factory_t *pau_real_factory;
static int pau_build_calls;
static int pau_builtin_created;
static SIXELSTATUS pau_get_factory(void **factory);

static int
pau_unavailable_build(sixel_png_chrm_matrix_t *out,
                       int64_t const grid[8], int requested, int adapt)
{
    sixel_png_chrm_matrix_t temporary;
    int result;

    result = sixel_png_chrm_build(&temporary, grid, requested, adapt);
    if (result == SIXEL_PNG_CHRM_OK &&
        temporary.selected == SIXEL_PNG_CHRM_512BIT) {
        ++pau_build_calls;
        return SIXEL_PNG_CHRM_UNSUPPORTED;
    }
    if (result == SIXEL_PNG_CHRM_OK) {
        *out = temporary;
    }
    return result;
}

#define SIXEL_PNG_TRANSFER_SRGB pau_transfer_srgb
#define SIXEL_PNG_TRANSFER_GAMA pau_transfer_gama
#define apng_blend_rect pau_000
#define apng_clear_rect pau_001
#define apng_decode_trace_is_enabled pau_002
#define apng_decode_trace_message pau_003
#define apng_replay_cache_prepare pau_004
#define apng_replay_cache_reset pau_005
#define apng_replay_cache_store_frame pau_006
#define append_chunk pau_007
#define append_shared_chunk pau_008
#define crc32_update pau_009
#define decode_png_rgba pau_010
#define emit_apng_frame pau_011
#define ensure_chunk_capacity pau_012
#define ensure_shared_capacity pau_013
#define g_sixel_loader_entries pau_014
#define g_sixel_loader_libpng_vtbl pau_015
#define g_sixel_loader_manager_vtbl pau_016
#define libpng_classify_stream pau_017
#define libpng_debug_trace_is_enabled pau_018
#define libpng_parse_animation_start_frame_no pau_019
#define libpng_parse_exif_orientation pau_020
#define libpng_resolve_animation_start_frame_no pau_021
#define load_apng_frames pau_022
#define load_png pau_023
#define load_with_libpng pau_024
#define loader_can_try_libpng pau_025
#define loader_delivery_callback pau_026
#define loader_delivery_state pau_027
#define loader_delivery_state_t pau_028
#define loader_manager_build_plan_from_resolution pau_029
#define loader_manager_entry_available pau_030
#define loader_manager_find_entry_index pau_031
#define loader_manager_get_entries pau_032
#define loader_manager_init_loader_suboptions pau_init_suboptions
#define loader_manager_lookup_token pau_034
#define loader_manager_parse_loader_order pau_parse_order
#define loader_manager_plan_contains pau_036
#define loader_manager_resolve_loader_suboptions pau_resolve_suboptions
#define loader_manager_status_allows_fallback pau_038
#define loader_manager_token_matches pau_039
#define parse_fctl pau_040
#define png_arithmetic_name pau_041
#define png_arithmetic_reason pau_042
#define png_clamp_unit pau_043
#define png_colorspace_from_pixelformat pau_044
#define png_convert_rgb16_rows_to_rgbfloat32 pau_045
#define png_decode_source_unit pau_046
#define png_decode_srgb_unit pau_047
#define png_encode_srgb_unit pau_048
#define png_error_callback pau_049
#define png_expand_background_sample_to_unit pau_050
#define png_resolve_background_unit pau_051
#define png_source_color_chunks pau_052
#define png_source_to_linear pau_053
#define read_be32 pau_054
#define read_palette pau_055
#define read_png pau_056
#define sixel_apng_canvas pau_057
#define sixel_apng_canvas_t pau_058
#define sixel_apng_frame_control pau_059
#define sixel_apng_frame_control_t pau_060
#define sixel_apng_replay_cache pau_061
#define sixel_apng_replay_cache_t pau_062
#define sixel_apng_state pau_063
#define sixel_apng_state_t pau_064
#define sixel_loader_libpng_component pau_065
#define sixel_loader_libpng_component_t pau_066
#define sixel_loader_libpng_load pau_067
#define sixel_loader_libpng_name pau_068
#define sixel_loader_libpng_new pau_new_libpng
#define sixel_loader_libpng_placeholder_function pau_070
#define sixel_loader_libpng_predicate pau_071
#define sixel_loader_libpng_ref pau_072
#define sixel_loader_libpng_setopt pau_073
#define sixel_loader_libpng_unref pau_074
#define sixel_loader_manager pau_075
#define sixel_loader_manager_append_chain pau_076
#define sixel_loader_manager_apply_component_options pau_077
#define sixel_loader_manager_build_chain_impl pau_078
#define sixel_loader_manager_clear_chain pau_079
#define sixel_loader_manager_format_worker_name pau_080
#define sixel_loader_manager_load_impl pau_081
#define sixel_loader_manager_new pau_new_manager
#define sixel_loader_manager_ref_impl pau_083
#define sixel_loader_manager_set_prefer_float32 pau_084
#define sixel_loader_manager_unref_impl pau_085
#define sixel_png_alpha pau_086
#define sixel_png_alpha_t pau_087
#define sixel_png_read_chunk pau_088
#define sixel_png_read_chunk_t pau_089
#define write_be32 pau_090
#define sixel_factory_get_default pau_get_factory
#define sixel_png_chrm_build pau_unavailable_build

/* Local signatures need forward declarations under the test namespace. */
SIXELSTATUS
pau_new_libpng(sixel_allocator_t *allocator,
                        void **ppcomponent);
SIXELSTATUS
pau_parse_order(
    char const *order,
    sixel_option_argument_list_resolution_t *resolution);
void
pau_init_suboptions(
    sixel_loader_suboptions_t *suboptions);
void
pau_resolve_suboptions(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_suboptions_t *suboptions);
size_t
pau_029(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_entry_t const *entries,
    size_t entry_count,
    sixel_loader_entry_t const **plan,
    size_t plan_capacity);
size_t
pau_032(sixel_loader_entry_t const **entries);
int
pau_030(char const *name);
int
pau_038(SIXELSTATUS status);
SIXEL_INTERNAL_API void
pau_084(sixel_loader_manager_t *manager,
                                        int prefer_float32);
SIXELSTATUS
pau_new_manager(sixel_allocator_t *allocator,
                         void **manager);
#undef LIBPNG_DEBUG_LOG
#include "src/loader-libpng.c"
#include "src/loader-manager.c"
#undef SIXEL_PNG_TRANSFER_SRGB
#undef SIXEL_PNG_TRANSFER_GAMA
#undef LIBPNG_DEBUG_LOG
#undef apng_blend_rect
#undef apng_clear_rect
#undef apng_decode_trace_is_enabled
#undef apng_decode_trace_message
#undef apng_replay_cache_prepare
#undef apng_replay_cache_reset
#undef apng_replay_cache_store_frame
#undef append_chunk
#undef append_shared_chunk
#undef crc32_update
#undef decode_png_rgba
#undef emit_apng_frame
#undef ensure_chunk_capacity
#undef ensure_shared_capacity
#undef g_sixel_loader_entries
#undef g_sixel_loader_libpng_vtbl
#undef g_sixel_loader_manager_vtbl
#undef libpng_classify_stream
#undef libpng_debug_trace_is_enabled
#undef libpng_parse_animation_start_frame_no
#undef libpng_parse_exif_orientation
#undef libpng_resolve_animation_start_frame_no
#undef load_apng_frames
#undef load_png
#undef load_with_libpng
#undef loader_can_try_libpng
#undef loader_delivery_callback
#undef loader_delivery_state
#undef loader_delivery_state_t
#undef loader_manager_build_plan_from_resolution
#undef loader_manager_entry_available
#undef loader_manager_find_entry_index
#undef loader_manager_get_entries
#undef loader_manager_init_loader_suboptions
#undef loader_manager_lookup_token
#undef loader_manager_parse_loader_order
#undef loader_manager_plan_contains
#undef loader_manager_resolve_loader_suboptions
#undef loader_manager_status_allows_fallback
#undef loader_manager_token_matches
#undef parse_fctl
#undef png_arithmetic_name
#undef png_arithmetic_reason
#undef png_clamp_unit
#undef png_colorspace_from_pixelformat
#undef png_convert_rgb16_rows_to_rgbfloat32
#undef png_decode_source_unit
#undef png_decode_srgb_unit
#undef png_encode_srgb_unit
#undef png_error_callback
#undef png_expand_background_sample_to_unit
#undef png_resolve_background_unit
#undef png_source_color_chunks
#undef png_source_to_linear
#undef read_be32
#undef read_palette
#undef read_png
#undef sixel_apng_canvas
#undef sixel_apng_canvas_t
#undef sixel_apng_frame_control
#undef sixel_apng_frame_control_t
#undef sixel_apng_replay_cache
#undef sixel_apng_replay_cache_t
#undef sixel_apng_state
#undef sixel_apng_state_t
#undef sixel_loader_libpng_component
#undef sixel_loader_libpng_component_t
#undef sixel_loader_libpng_load
#undef sixel_loader_libpng_name
#undef sixel_loader_libpng_new
#undef sixel_loader_libpng_placeholder_function
#undef sixel_loader_libpng_predicate
#undef sixel_loader_libpng_ref
#undef sixel_loader_libpng_setopt
#undef sixel_loader_libpng_unref
#undef sixel_loader_manager
#undef sixel_loader_manager_append_chain
#undef sixel_loader_manager_apply_component_options
#undef sixel_loader_manager_build_chain_impl
#undef sixel_loader_manager_clear_chain
#undef sixel_loader_manager_format_worker_name
#undef sixel_loader_manager_load_impl
#undef sixel_loader_manager_new
#undef sixel_loader_manager_ref_impl
#undef sixel_loader_manager_set_prefer_float32
#undef sixel_loader_manager_unref_impl
#undef sixel_png_alpha
#undef sixel_png_alpha_t
#undef sixel_png_read_chunk
#undef sixel_png_read_chunk_t
#undef write_be32
#undef sixel_factory_get_default
#undef sixel_png_chrm_build

static void
pau_factory_ref(sixel_factory_t *factory)
{
    (void)factory;
}

static void
pau_factory_unref(sixel_factory_t *factory)
{
    (void)factory;
}

static SIXELSTATUS
pau_factory_create(sixel_factory_t *factory, char const *name,
                    sixel_allocator_t *allocator, void **out)
{
    (void)factory;
    if (strcmp(name, "loader/libpng") == 0) {
        return pau_new_libpng(allocator, out);
    }
    if (strcmp(name, "loader/manager") == 0) {
        return pau_new_manager(allocator, out);
    }
    if (strcmp(name, "loader/builtin") == 0) {
        ++pau_builtin_created;
    }
    return pau_real_factory->vtbl->create(pau_real_factory, name,
                                          allocator, out);
}

static SIXELSTATUS
pau_get_factory(void **factory)
{
    static sixel_factory_vtbl_t const vtbl = {
        pau_factory_ref, pau_factory_unref, pau_factory_create
    };
    static sixel_factory_t instance = {&vtbl};

    *factory = &instance;
    return SIXEL_OK;
}

static SIXELSTATUS
pau_capture(sixel_frame_t *frame, void *context)
{
    int *calls;

    (void)frame;
    calls = (int *)context;
    ++*calls;
    return SIXEL_OK;
}
#endif

int
test_loader_0211_png_arith_unavailable(int argc, char **argv)
{
#if HAVE_LIBPNG
    sixel_loader_manager_t *manager;
    sixel_loader_component_t *selected;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_option_argument_list_resolution_t resolution;
    sixel_loader_suboptions_t suboptions;
    sixel_loader_suboptions_t before_suboptions;
    sixel_loader_manager_build_request_t request;
    SIXELSTATUS status;
    unsigned char original[sizeof(png_arith_png_5)];
    int calls;
    int result;

    (void)argv;
    if (argc != 1) {
        return 1;
    }
    manager = NULL;
    allocator = NULL;
    chunk = NULL;
    selected = NULL;
    calls = 0;
    result = 1;
    pau_build_calls = 0;
    pau_builtin_created = 0;
    sixel_option_init_argument_list_resolution(&resolution);
    memset(&request, 0, sizeof(request));
    status = sixel_factory_get_default((void **)&pau_real_factory);
    if (SIXEL_FAILED(status)) {
        return 1;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = pau_new_manager(allocator, (void **)&manager);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_chunk_create_from_memory(&chunk, png_arith_png_5,
        sizeof(png_arith_png_5), NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    memcpy(original, sixel_chunk_get_buffer(chunk), sizeof(original));
    /* Establish that the following loader could otherwise deliver a frame. */
    status = pau_parse_order("builtin:cms_engine=none", &resolution);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pau_resolve_suboptions(&resolution, &suboptions);
    request.resolution = &resolution;
    request.suboptions = &suboptions;
    request.require_static = 1;
    request.reqcolors = 256;
    status = manager->vtbl->build_chain(manager, &request);
    if (SIXEL_FAILED(status) || manager->vtbl->load(manager, chunk,
            &selected, pau_capture, &calls) != SIXEL_OK || calls != 1) {
        goto end;
    }
    calls = 0;
    sixel_option_free_argument_list_resolution(&resolution);
    status = pau_parse_order(
        "libpng:cms_engine=builtin:chrm_arithmetic=512bit,builtin",
        &resolution);
    if (SIXEL_FAILED(status) || resolution.has_trailing_bang) {
        goto end;
    }
    pau_resolve_suboptions(&resolution, &suboptions);
    memcpy(&before_suboptions, &suboptions, sizeof(suboptions));
    status = manager->vtbl->build_chain(manager, &request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = manager->vtbl->load(manager, chunk, &selected, pau_capture,
                                  &calls);
    if (status != SIXEL_NOT_IMPLEMENTED || selected != NULL || calls != 0 ||
        pau_build_calls != 1 || pau_builtin_created < 2 ||
        strstr(sixel_helper_get_additional_message(),
            "selected 512bit cHRM arithmetic is unavailable") == NULL ||
        memcmp(original, sixel_chunk_get_buffer(chunk), sizeof(original)) ||
        memcmp(&suboptions, &before_suboptions, sizeof(suboptions))) {
        goto end;
    }
    /* The same chain retains its forced setting. Failure cannot rewrite the
     * caller's options to double or make a later invocation deliver pixels. */
    status = manager->vtbl->load(manager, chunk, &selected, pau_capture,
                                  &calls);
    result = status != SIXEL_NOT_IMPLEMENTED || calls != 0 ||
        selected != NULL || pau_build_calls != 2;
end:
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    if (manager != NULL) {
        manager->vtbl->unref(manager);
    }
    sixel_allocator_unref(allocator);
    sixel_option_free_argument_list_resolution(&resolution);
    pau_real_factory->vtbl->unref(pau_real_factory);
    return result;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
