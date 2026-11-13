/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <stdint.h>
#include "fixture.h"
#include "src/lookup-policy-gpu.h"

/* Decorate only the program ABI: no test reads a policy's private table.
 * The injected shader checks independent, CPU-derived quarter-code queries
 * before returning the real lookup result to the production GPU LSO2 dither. */
typedef struct probe_plan {
    sixel_gpu_lookup_plan_interface_t iface;
    unsigned int refs;
    sixel_gpu_lookup_plan_interface_t *subject;
    sixel_gpu_lookup_program_view_t view;
    char const **chunks;
    char *suffix;
    sixel_gpu_lookup_batch_dispatch_t batch;
    unsigned int *builds;
} probe_plan_t;

typedef struct probe_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_gpu_lookup_preparer_interface_t *subject;
    char const *suffix;
    int batch_fault;
    int fractional_fault;
    unsigned int *builds;
} probe_preparer_t;

static void
probe_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    ++((probe_plan_t *)self)->refs;
}

static void
probe_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    probe_plan_t *p;

    p = (probe_plan_t *)self;
    if (--p->refs) return;
    p->subject->vtbl->unref(p->subject);
    free(p->chunks);
    free(p->suffix);
    free(p);
}

static SIXELSTATUS
probe_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    *view = ((probe_plan_t const *)self)->view;
    return SIXEL_OK;
}

static SIXELSTATUS
probe_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **binding)
{
    probe_plan_t *p;
    sixel_gpu_lookup_binding_view_t view;
    SIXELSTATUS status;

    p = (probe_plan_t *)self;
    status = p->subject->vtbl->begin(p->subject, binding);
    if (status == SIXEL_OK && p->builds != NULL) {
        status = (*binding)->vtbl->get_view(*binding, &view);
        if (status == SIXEL_OK) *p->builds += view.needs_build != 0;
    }
    return status;
}

static sixel_gpu_lookup_plan_vtbl_t const probe_plan_vtbl = {
    probe_ref, probe_unref, probe_describe, probe_begin
};

static void
probe_provider_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    (void)self;
}

static void
probe_provider_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    (void)self;
}

static SIXELSTATUS
probe_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    probe_preparer_t const *provider;
    probe_plan_t *p;
    size_t i;
    size_t suffix_size;
    SIXELSTATUS status;

    provider = (probe_preparer_t const *)self;
    *result = NULL;
    p = calloc(1, sizeof(*p));
    if (p == NULL) return SIXEL_BAD_ALLOCATION;
    p->iface.vtbl = &probe_plan_vtbl;
    p->refs = 1;
    p->builds = provider->builds;
    status = provider->subject->vtbl->prepare(provider->subject, request,
        device, &p->subject);
    if (status != SIXEL_OK) { free(p); return status; }
    status = p->subject->vtbl->describe(p->subject, &p->view);
    if (status != SIXEL_OK) goto fail;
    if (p->view.query_kind != SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL) {
        status = SIXEL_LOGIC_ERROR;
        goto fail;
    }
    if (provider->batch_fault != 0) {
        if (p->view.batch == NULL) { status = SIXEL_LOGIC_ERROR; goto fail; }
        p->batch = *p->view.batch;
        p->view.batch = &p->batch;
        if (provider->batch_fault == 1) p->batch.tile[0] = 0;
        if (provider->batch_fault == 2) p->batch.threads[0] = SIZE_MAX;
        if (provider->batch_fault == 3) {
            p->chunks = calloc(1,sizeof(*p->chunks));
            if (p->chunks == NULL) { status = SIXEL_BAD_ALLOCATION; goto fail; }
            p->chunks[0] = "#include <metal_stdlib>\nusing namespace metal;\n"
                "kernel void bad_batch(constant uint& n [[buffer(1)]],"
                "device uint *out [[buffer(5)]],"
                "constant uint2& e [[buffer(6)]],"
                "uint2 tile [[threadgroup_position_in_grid]],"
                "uint lane [[thread_index_in_threadgroup]]) {"
                "for(uint h=0;h<2;++h) { uint p=lane+32*h;"
                "uint2 xy=tile*8+uint2(p%8,p/8);"
                "if(all(xy<e)) out[xy.y*e.x+xy.x]=n; }}\n";
            p->view.chunks = p->chunks;
            p->view.chunk_count = 1;
            p->batch.entry_point = "bad_batch";
            if (strcmp(p->view.program_format,
                    "vulkan.lookup-rgb8.v1") == 0) {
                p->chunks[0] = "#define SIXEL_LOOKUP_BATCH 1\n"
                    "void lookup_rgb8_batch(vec4 q[2], uint n,"
                    "out uvec2 indexes) { indexes=uvec2(n); }\n";
                p->batch.entry_point = "lookup_rgb8_batch";
            }
        }
        *result = &p->iface;
        return SIXEL_OK;
    }
    if (provider->suffix == NULL) {
        *result = &p->iface;
        return SIXEL_OK;
    }
    suffix_size = strlen(provider->suffix) + 1u;
    p->suffix = malloc(suffix_size);
    p->chunks = calloc(p->view.chunk_count+2, sizeof(*p->chunks));
    if (p->suffix == NULL || p->chunks == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto fail;
    }
    /* Copy the terminator within the exact allocation made above. */
    memcpy(p->suffix, provider->suffix, suffix_size);
    p->chunks[0] = provider->fractional_fault
        ? "#define lookup_rgb8_fractional subject_fractional\n"
          "#define SIXEL_TEST_PROBE_FAULT 1u\n"
        : "#define lookup_rgb8_fractional subject_fractional\n"
          "#define SIXEL_TEST_PROBE_FAULT 0u\n";
    for (i = 0; i < p->view.chunk_count; ++i)
        p->chunks[i+1] = p->view.chunks[i];
    p->chunks[i+1] = p->suffix;
    p->view.chunks = p->chunks;
    p->view.chunk_count += 2;
    *result = &p->iface;
    return SIXEL_OK;
fail:
    probe_unref(&p->iface);
    return status;
}

static sixel_gpu_lookup_preparer_vtbl_t const probe_provider_vtbl = {
    probe_provider_ref, probe_provider_unref, probe_prepare
};

/* Quarter coordinates have exact float squared distances: multiplying by
 * sixteen yields integers below 3*1020^2 < 2^24. Independent double
 * references follow each policy's exact or approximate query rule. */
int
lso2_gpu_fractional_config_check(
    sixel_lookup_policy_configuration_t const *configuration, int ties)
{
    lso2_gpu_fixture_t f;
    probe_preparer_t probe;
    sixel_palette_entries_request_t entries;
    sixel_wire_palette_request_t wire;
    sixel_wire_color_definition_t definitions[41];
    unsigned char colors[123];
    char suffix[16384];
    char const *format, *vector, *arguments;
    size_t used;
    double q[3];
    unsigned int i, c, count, seed, expected, value;
    int outcome, written, vulkan;

    memset(&probe, 0, sizeof(probe));
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    format = f.device->vtbl->program_format(f.device);
    vulkan = strcmp(format, "vulkan.lookup-rgb8.v1") == 0;
    /* Both scalar shader ABIs wrap the selected policy itself; admitting
     * a different available lookup would invalidate its oracle comparison. */
    if ((!vulkan && strcmp(format, "metal.lookup-rgb8.v1") != 0) ||
            !lso2_gpu_fixture_serves(&f, configuration->kind)) {
        outcome = 77;
        goto end;
    }
#if !defined(HAVE_SHADERC)
    /* The decorated source is deliberately absent from the production
     * SPIR-V matrix, so a Vulkan probe needs runtime GLSL compilation. */
    if (vulkan) {
        fprintf(stderr, "Vulkan fractional probe requires libshaderc\n");
        outcome = 77;
        goto end;
    }
#endif
    outcome = 1;
    count = ties ? 5 : 40;
    memset(definitions, 0, sizeof(definitions));
    memset(colors, 0, sizeof(colors));
    for (i = 0; i < count; ++i) {
        definitions[i].register_index = i;
        definitions[i].type = 2;
        for (c = 0; c < 3; ++c) {
            value = i < 5 ? (i == c+1 || (i == 4 && c == 0)
                    ? (c == 0 ? 6 : c == 1 ? 12 : 2) : 0)
                : i == count-1 ? 100 : 10+(i*37+c*29)%91;
            definitions[i].fields[c] = value;
            colors[3*i+c] = (unsigned char)((value*255+50)/100);
        }
    }
    definitions[count].register_index = count;
    definitions[count].type = 2;
    entries.entries = colors;
    entries.colors = count+1;
    entries.depth = 3;
    LSO2_GPU_CHECK(f.palette->vtbl->init_entries(f.palette, &entries)
        == SIXEL_OK);
    sixel_wire_palette_unref(f.wire);
    f.wire = NULL;
    memset(&wire, 0, sizeof(wire));
    wire.definitions = definitions;
    wire.count = count+1;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    LSO2_GPU_CHECK(sixel_wire_palette_new(&f.wire, &wire, f.allocator)
        == SIXEL_OK);
    f.prepare.output.palette = f.wire;
    f.prepare.output.keycolor = (int)count;
    f.configuration.options.ncolors = (int)count+1;
    for (i = 0; i < LSO2_GPU_PIXELS; ++i) {
        f.pixels[3*i] = (unsigned char)i;
        f.pixels[3*i+1] = (unsigned char)(i*17);
        f.pixels[3*i+2] = (unsigned char)(i*73);
    }
    vector = vulkan ? "vec4" : "float4";
    arguments = vulkan ? "n" : "p,n,c,w";
    used = (size_t)snprintf(suffix, sizeof(suffix),
        "\n#undef lookup_rgb8_fractional\n%s\n", vulkan
            ? "const vec4 probes[128] = vec4[128]("
            : "constant float4 probes[128] = {");
    seed = 7;
    for (i = 0; i < 128; ++i) {
        for (c = 0; c < 3; ++c) {
            seed = seed*1664525U+1013904223U;
            q[c] = (double)((seed>>8)%1021)/4;
            if (i < 32) q[c] = i*8+(c == 0 ? 7.75 : c == 1 ? 0 : 4.25);
            if (q[c] > 255) q[c] = 255;
            if (ties) q[c] = c == i%3 ? colors[3*(c+1)+c]*.5 : 0;
        }
        if ((i == 0 || i == 1) && !ties) {
            q[0] = i == 0 ? 7.5 : 7.75;
            q[1] = q[2] = 0;
        }
        /* The green Voronoi boundary is 15.5: a byte cell ending at 15
         * cannot certify the upper fractional strip of this 16-code cell. */
        if (i == 2 && !ties) { q[0] = q[2] = 0; q[1] = 15.75; }
        if (i == 3 && ties) { q[0] = 15; q[1] = q[2] = 0; }
        expected = lso2_gpu_fractional_reference(configuration,colors,count,q);
        written = snprintf(suffix+used, sizeof(suffix)-used,
            "%s(%.2ff,%.2ff,%.2ff,%u.0f)%s\n", vector,
            q[0],q[1],q[2],expected, i+1 < 128 ? "," : "");
        LSO2_GPU_CHECK(written > 0 && (size_t)written < sizeof(suffix)-used);
        used += (size_t)written;
    }
    written = snprintf(suffix+used, sizeof(suffix)-used, "%s\n", vulkan
        ? ");\nuint lookup_rgb8_fractional(vec3 q,uint n) {"
        : "};\nstatic uint lookup_rgb8_fractional(float3 q,"
          " constant uchar *p,uint n,constant uchar *c,"
          " device const uchar *w) {");
    LSO2_GPU_CHECK(written > 0 && (size_t)written < sizeof(suffix)-used);
    used += (size_t)written;
    /* Check every witness independently of the dither's query values.
     * Flipping only the last expected index proves the loop reaches
     * its end and that the consumer rejects a failed probe. */
    written = snprintf(suffix+used, sizeof(suffix)-used,
        "for (uint i=0u;i<128u;++i) {\n"
        "%s probe=probes[i];\n"
        "uint expected=uint(probe.w) ^ "
        "(i==127u ? SIXEL_TEST_PROBE_FAULT : 0u);\n"
        "if (subject_fractional(probe.xyz,%s) != expected) return n;\n"
        "}\nreturn subject_fractional(q,%s);\n}\n",
        vector, arguments, arguments);
    LSO2_GPU_CHECK(written > 0 && (size_t)written < sizeof(suffix)-used);
    LSO2_GPU_CHECK(sixel_gpu_lookup_select(f.allocator, configuration,
        &probe.subject) == SIXEL_OK);
    probe.iface.vtbl = &probe_provider_vtbl;
    probe.suffix = suffix;
    f.prepare.gpu_lookup = &probe.iface;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    /* A different decorated source must not reuse an uninstrumented
     * prebuilt program or the successful probe's cached pipeline. */
    probe.fractional_fault = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_BAD_ARGUMENT);
    LSO2_GPU_CHECK(sixel_indexed_frame_get_state(f.image) !=
        SIXEL_INDEXED_COMPLETE);
    probe.fractional_fault = 0;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    if (probe.subject != NULL) probe.subject->vtbl->unref(probe.subject);
    return outcome;
}

int
lso2_gpu_fractional_check(int kind, int ties)
{
    sixel_lookup_policy_configuration_t configuration;

    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = kind;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    return lso2_gpu_fractional_config_check(&configuration, ties);
}

/* Keep the prepared plan alive across solves. Successful completion must
 * return its table to the shared cache and the next solve must borrow that
 * valid table, rather than rebuild it or read an empty workspace. */
int
lso2_gpu_fractional_reuse_check(int kind)
{
    lso2_gpu_fixture_t f;
    probe_preparer_t probe;
    sixel_lookup_policy_configuration_t configuration;
    unsigned int builds;
    int outcome;

    memset(&probe, 0, sizeof(probe));
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    /* Counting builds uses only the binding ABI, so it needs neither a
     * backend-specific shader nor a runtime compiler. */
    if (!lso2_gpu_fixture_serves(&f, kind)) {
        outcome = 77;
        goto end;
    }
    outcome = 1;
    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = kind;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    configuration.fhedt_resolution = 64;
    configuration.fhedt_refine = 1;
    configuration.fhedt_weights[0] = 1;
    configuration.fhedt_weights[1] = 1;
    configuration.fhedt_weights[2] = 1;
    LSO2_GPU_CHECK(sixel_gpu_lookup_select(f.allocator, &configuration,
        &probe.subject) == SIXEL_OK);
    probe.iface.vtbl = &probe_provider_vtbl;
    probe.builds = &builds;
    builds = 0;
    f.prepare.gpu_lookup = &probe.iface;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    builds = 0;
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(builds == 0);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    if (probe.subject != NULL) probe.subject->vtbl->unref(probe.subject);
    return outcome;
}

/* Interleave independently prepared policies while the first frozen plan
 * remains alive. Each cold table must build once; neither completed table
 * may evict the other policy's workspace, even through another preparer. */
int
lso2_gpu_fractional_policy_isolation_check(int kind)
{
    lso2_gpu_fixture_t f[2];
    probe_preparer_t probe[2];
    sixel_lookup_policy_configuration_t configuration;
    unsigned int builds[2];
    unsigned char indexes[2][LSO2_GPU_PIXELS];
    int outcome, i, policy;

    memset(f, 0, sizeof(f));
    memset(probe, 0, sizeof(probe));
    memset(builds, 0, sizeof(builds));
    memset(&configuration, 0, sizeof(configuration));
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    for (i = 0; i < 2; ++i) {
        policy = i == 0 ? kind : kind == SIXEL_LUT_POLICY_5BIT
            ? SIXEL_LUT_POLICY_6BIT : SIXEL_LUT_POLICY_5BIT;
        outcome = lso2_gpu_fixture_open(&f[i], 1);
        if (outcome != 0) goto end;
        if (!lso2_gpu_fixture_serves(&f[i], policy)) {
            outcome = 77;
            goto end;
        }
        outcome = 1;
        configuration.kind = policy;
        LSO2_GPU_CHECK(sixel_gpu_lookup_select(f[i].allocator,
            &configuration, &probe[i].subject) == SIXEL_OK);
        probe[i].iface.vtbl = &probe_provider_vtbl;
        probe[i].builds = &builds[i];
        f[i].prepare.gpu_lookup = &probe[i].iface;
        LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f[i]) == SIXEL_OK);
        /* Preparation probes an interrupted lease to reserve capacity;
         * count only the actual solve's build request below. */
        builds[i] = 0;
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f[i]) == SIXEL_OK);
        LSO2_GPU_CHECK(builds[i] == 1);
        memcpy(indexes[i], f[i].image->indexes, sizeof(indexes[i]));
    }
    for (i = 0; i < 2; ++i) {
        builds[i] = 0;
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f[i]) == SIXEL_OK);
        LSO2_GPU_CHECK(builds[i] == 0);
        LSO2_GPU_CHECK(memcmp(indexes[i], f[i].image->indexes,
            sizeof(indexes[i])) == 0);
    }
    outcome = 0;
end:
    for (i = 0; i < 2; ++i) {
        lso2_gpu_fixture_close(&f[i]);
        if (probe[i].subject != NULL)
            probe[i].subject->vtbl->unref(probe[i].subject);
    }
    return outcome;
}

/* The consumer must reject malformed capabilities before execution and
 * invalidate, rather than publish, an out-of-range collective result. */
int
lso2_gpu_batch_fault_check(int fault)
{
    lso2_gpu_fixture_t f;
    probe_preparer_t probe;
    sixel_lookup_policy_configuration_t configuration;
    int outcome;
    SIXELSTATUS status;

    memset(&probe,0,sizeof(probe));
    outcome = lso2_gpu_fixture_open(&f,1);
    if (outcome != 0) goto end;
#if !defined(HAVE_SHADERC)
    if (fault == 3 && strcmp(f.device->vtbl->program_format(f.device),
            "vulkan.lookup-rgb8.v1") == 0) {
        outcome = 77;
        goto end;
    }
#endif
    outcome = 1;
    memset(&configuration,0,sizeof(configuration));
    configuration.kind = SIXEL_LUT_POLICY_BLOCKCULL;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    LSO2_GPU_CHECK(sixel_gpu_lookup_select(f.allocator,&configuration,
        &probe.subject) == SIXEL_OK);
    probe.iface.vtbl = &probe_provider_vtbl;
    probe.batch_fault = fault;
    f.prepare.gpu_lookup = &probe.iface;
    status = lso2_gpu_fixture_prepare(&f);
    if (fault == 1) LSO2_GPU_CHECK(status == SIXEL_BAD_ARGUMENT);
    else if (fault == 2) LSO2_GPU_CHECK(status == SIXEL_FEATURE_ERROR);
    else {
        LSO2_GPU_CHECK(status == SIXEL_OK);
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_BAD_ARGUMENT);
        LSO2_GPU_CHECK(sixel_indexed_frame_get_state(f.image) !=
            SIXEL_INDEXED_COMPLETE);
    }
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    if (probe.subject != NULL) probe.subject->vtbl->unref(probe.subject);
    return outcome;
}
