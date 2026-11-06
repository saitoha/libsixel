/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "fixture.h"
#include "src/lookup-policy-gpu.h"

/* Decorate only the program ABI: no test reads a policy's private table.
 * The injected shader checks independent, CPU-derived quarter-code queries
 * before returning the real lookup result to the production phased dither. */
typedef struct probe_plan {
    sixel_gpu_lookup_plan_interface_t iface;
    unsigned int refs;
    sixel_gpu_lookup_plan_interface_t *subject;
    sixel_gpu_lookup_program_view_t view;
    char const **chunks;
    char *suffix;
} probe_plan_t;

typedef struct probe_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_gpu_lookup_preparer_interface_t *subject;
    char const *suffix;
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

    p = (probe_plan_t *)self;
    return p->subject->vtbl->begin(p->subject, binding);
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
    SIXELSTATUS status;

    provider = (probe_preparer_t const *)self;
    *result = NULL;
    p = calloc(1, sizeof(*p));
    if (p == NULL) return SIXEL_BAD_ALLOCATION;
    p->iface.vtbl = &probe_plan_vtbl;
    p->refs = 1;
    status = provider->subject->vtbl->prepare(provider->subject, request,
        device, &p->subject);
    if (status != SIXEL_OK) { free(p); return status; }
    status = p->subject->vtbl->describe(p->subject, &p->view);
    if (status != SIXEL_OK) goto fail;
    if (p->view.query_kind != SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL) {
        status = SIXEL_LOGIC_ERROR;
        goto fail;
    }
    p->suffix = malloc(strlen(provider->suffix)+1);
    p->chunks = calloc(p->view.chunk_count+2, sizeof(*p->chunks));
    if (p->suffix == NULL || p->chunks == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto fail;
    }
    strcpy(p->suffix, provider->suffix);
    p->chunks[0] = "#define lookup_rgb8_fractional subject_fractional\n";
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
 * sixteen yields integers below 3*1020^2 < 2^24. Double enumeration is an
 * independent nearest-color oracle including exact lowest-index ties. */
int
phased_fractional_check(int kind, int ties)
{
    phased_fixture_t f;
    probe_preparer_t probe;
    sixel_lookup_policy_configuration_t configuration;
    sixel_palette_entries_request_t entries;
    sixel_wire_palette_request_t wire;
    sixel_wire_color_definition_t definitions[41];
    unsigned char colors[123];
    char suffix[16384];
    size_t used;
    double q[3], gap, distance, minimum;
    unsigned int i, j, c, count, seed, expected, value;
    int outcome, written;

    memset(&probe, 0, sizeof(probe));
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
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
    PHASED_CHECK(f.palette->vtbl->init_entries(f.palette, &entries)
        == SIXEL_OK);
    sixel_wire_palette_unref(f.wire);
    f.wire = NULL;
    memset(&wire, 0, sizeof(wire));
    wire.definitions = definitions;
    wire.count = count+1;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    PHASED_CHECK(sixel_wire_palette_new(&f.wire, &wire, f.allocator)
        == SIXEL_OK);
    f.prepare.output.palette = f.wire;
    f.prepare.output.keycolor = (int)count;
    f.configuration.options.ncolors = (int)count+1;
    for (i = 0; i < PHASED_PIXELS; ++i) {
        f.pixels[3*i] = (unsigned char)i;
        f.pixels[3*i+1] = (unsigned char)(i*17);
        f.pixels[3*i+2] = (unsigned char)(i*73);
    }
    used = (size_t)snprintf(suffix, sizeof(suffix),
        "\n#undef lookup_rgb8_fractional\n"
        "constant float4 probes[128] = {\n");
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
        minimum = 1e30;
        expected = 0;
        for (j = 0; j < count; ++j) {
            distance = 0;
            for (c = 0; c < 3; ++c) {
                gap = q[c]-colors[3*j+c];
                distance += gap*gap;
            }
            if (distance < minimum) { minimum = distance; expected = j; }
        }
        written = snprintf(suffix+used, sizeof(suffix)-used,
            "float4(%.2ff,%.2ff,%.2ff,%u.0f),\n", q[0],q[1],q[2],expected);
        PHASED_CHECK(written > 0 && (size_t)written < sizeof(suffix)-used);
        used += (size_t)written;
    }
    written = snprintf(suffix+used, sizeof(suffix)-used,
        "};\nstatic uint lookup_rgb8_fractional(float3 q,"
        " constant uchar *p,uint n,constant uchar *c,"
        " device const uchar *w) {\n"
        "float4 probe = probes[uint(q.x) %% 128u];\n"
        "if (subject_fractional(probe.xyz,p,n,c,w) != uint(probe.w)) "
        "return n;\nreturn subject_fractional(q,p,n,c,w);\n}\n");
    PHASED_CHECK(written > 0 && (size_t)written < sizeof(suffix)-used);
    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = kind;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    PHASED_CHECK(sixel_gpu_lookup_select(f.allocator, &configuration,
        &probe.subject) == SIXEL_OK);
    probe.iface.vtbl = &probe_provider_vtbl;
    probe.suffix = suffix;
    f.prepare.gpu_lookup = &probe.iface;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_OK);
    outcome = 0;
end:
    phased_fixture_close(&f);
    if (probe.subject != NULL) probe.subject->vtbl->unref(probe.subject);
    return outcome;
}
