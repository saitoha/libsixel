/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/compat_stub.h"

/* Track identities, not job counts or timing-dependent throughput. */
static int
pipeline_remember(unsigned long long *ids, int *count,
                  unsigned long long thread)
{
    int i;

    for (i = 0; i < *count; ++i) {
        if (ids[i] == thread) {
            return 1;
        }
    }
    if (*count == 8) {
        return 0;
    }
    ids[(*count)++] = thread;
    return 1;
}

int
pipeline_worker_counts(char const *path, char const *plan, int automatic)
{
    FILE *file;
    char line[2048];
    char const *field;
    unsigned long long thread;
    unsigned long long dithers[8];
    unsigned long long encoders[8];
    unsigned long long finished[8];
    unsigned int session;
    unsigned int pipeline_session;
    int planned_dither;
    int planned_encode;
    int dither_count;
    int encode_count;
    int finish_count;
    int configured;
    int grew;
    int ok;

    field = strstr(plan, "threads: dither=");
    if (field == NULL || sscanf(field, "threads: dither=%d encode=%d",
            &planned_dither, &planned_encode) != 2 ||
            planned_dither != 6 || planned_encode != 2 ||
            strstr(plan, "bands=75 queue=6 mode=pipeline") == NULL ||
            strstr(plan, "band_height=78 overlap=0") == NULL ||
            strstr(plan, automatic ? "workers: total=8 main=4 palette=4" :
                "workers: total=8 main=8 palette=0") == NULL) {
        fprintf(stderr, "Unexpected pipeline plan:\n%s\n", plan);
        return EXIT_FAILURE;
    }
    file = sixel_compat_fopen(path, "rb");
    if (file == NULL) {
        return EXIT_FAILURE;
    }
    dither_count = encode_count = finish_count = configured = grew = 0;
    pipeline_session = 0;
    ok = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        field = strstr(line, "\"thread\":");
        if (field == NULL || sscanf(field, "\"thread\":%llu",
                &thread) != 1) {
            ok = 0;
            break;
        }
        field = strstr(line, "\"session_id\":");
        if (field == NULL || sscanf(field, "\"session_id\":%u",
                &session) != 1) {
            ok = 0;
            break;
        }
        if (strstr(line, "\"worker\":\"pipeline\"") != NULL &&
                strstr(line, "\"event\":\"configure\"") != NULL) {
            pipeline_session = session;
            ++configured;
        }
        if (configured != 1 || session != pipeline_session) {
            continue;
        }
        if (strstr(line, "\"worker\":\"dither\"") != NULL &&
                strstr(line, "\"role\":\"worker\"") != NULL) {
            if (strstr(line, "\"event\":\"start\"") != NULL) {
                ok &= pipeline_remember(dithers, &dither_count, thread);
            } else if (strstr(line, "\"event\":\"finish\"") != NULL) {
                ok &= pipeline_remember(finished, &finish_count, thread);
            }
        }
        if (strstr(line, "\"worker\":\"encode\"") == NULL) {
            continue;
        }
        /*
         * Only the overlap phase has the advertised initial encode budget.
         * Growth happens after every dither worker finishes; its notification
         * follows thread creation, so it cannot delimit initial workers.
         */
        if (finish_count < planned_dither &&
                strstr(line, "\"role\":\"worker\"") != NULL &&
                strstr(line, "\"event\":\"worker_start\"") != NULL) {
            ok &= pipeline_remember(encoders, &encode_count, thread);
        }
        if (strstr(line, "\"event\":\"grow_workers\"") != NULL) {
            ok &= finish_count == planned_dither;
            ++grew;
        }
    }
    ok &= !ferror(file);
    fclose(file);
    ok &= configured == 1 && grew == 1;
    ok &= dither_count == planned_dither && finish_count == planned_dither;
    ok &= encode_count == planned_encode;
    if (!ok) {
        fprintf(stderr, "Pipeline workers: planned=%d/%d observed=%d/%d "
                "finished=%d configure=%d grow=%d\n", planned_dither,
                planned_encode, dither_count, encode_count, finish_count,
                configured, grew);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
