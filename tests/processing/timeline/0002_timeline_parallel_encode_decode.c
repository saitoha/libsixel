/*
 * SPDX-License-Identifier: MIT
 *
 * Concurrent timeline logging test.  Multiple encoder/decoder objects share the
 * process timeline_writer service.  The producer only writes and flushes; the
 * TAP file invokes the JSONL verifier in a fresh test_runner process.  MSVC's
 * CRT can make immediate same-process readback brittle even after fclose().
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/compat_stub.h"
#include "src/threading.h"

#define TIMELINE_PARALLEL_WORKERS 4
#define TIMELINE_MAX_SESSIONS 16
#if SIXEL_ENABLE_THREADS

typedef struct timeline_parallel_sync {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    int worker_count;
    int ready_count;
    int released;
} timeline_parallel_sync_t;

typedef struct timeline_parallel_job {
    timeline_parallel_sync_t *sync;
    char const *encode_input_path;
    char const *decode_input_path;
    char const *output_path;
    int kind;
    int index;
} timeline_parallel_job_t;

static char const *
timeline_null_output_path(void)
{
#if defined(_WIN32)
    return "NUL";
#else
    return "/dev/null";
#endif
}

static int
timeline_build_source_path(char *path,
                           size_t path_size,
                           char const *relative_path)
{
    char const *source_root;
    int written;

    source_root = sixel_compat_getenv("TOP_SRCDIR");
    if (source_root == NULL || source_root[0] == '\0') {
        source_root = ".";
    }

    written = snprintf(path, path_size, "%s/%s", source_root, relative_path);
    if (written < 0 || (size_t)written >= path_size) {
        return 0;
    }

    return 1;
}

static void
timeline_parallel_release(timeline_parallel_sync_t *sync)
{
    sixel_mutex_lock(&sync->mutex);
    sync->released = 1;
    sixel_cond_broadcast(&sync->cond);
    sixel_mutex_unlock(&sync->mutex);
}

static int
timeline_parallel_wait_for_release(timeline_parallel_sync_t *sync)
{
    sixel_mutex_lock(&sync->mutex);
    sync->ready_count = sync->ready_count + 1;
    if (sync->ready_count == sync->worker_count) {
        sixel_cond_broadcast(&sync->cond);
    }
    while (sync->released == 0) {
        sixel_cond_wait(&sync->cond, &sync->mutex);
    }
    sixel_mutex_unlock(&sync->mutex);

    return 1;
}

static int
timeline_run_encode_job(timeline_parallel_job_t const *job)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    int success;

    allocator = NULL;
    encoder = NULL;
    success = 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_OUTPUT,
                                  job->output_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode(encoder, job->encode_input_path);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "timeline parallel encode job %d failed: %s\n",
                job->index,
                sixel_helper_get_additional_message());
        goto end;
    }

    success = 1;

end:
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return success;
}

static int
timeline_run_decode_job(timeline_parallel_job_t const *job)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_decoder_t *decoder;
    int success;

    allocator = NULL;
    decoder = NULL;
    success = 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_decoder_setopt(decoder,
                                  SIXEL_OPTFLAG_INPUT,
                                  job->decode_input_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_decoder_setopt(decoder,
                                  SIXEL_OPTFLAG_OUTPUT,
                                  job->output_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_decoder_decode(decoder);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "timeline parallel decode job %d failed: %s\n",
                job->index,
                sixel_helper_get_additional_message());
        goto end;
    }

    success = 1;

end:
    if (decoder != NULL) {
        sixel_decoder_unref(decoder);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return success;
}

static int
timeline_parallel_worker(void *arg)
{
    timeline_parallel_job_t const *job;
    int success;

    job = (timeline_parallel_job_t const *)arg;
    success = 0;
    if (job == NULL || job->sync == NULL) {
        return EXIT_FAILURE;
    }
    (void)timeline_parallel_wait_for_release(job->sync);

    if (job->kind == 0) {
        success = timeline_run_encode_job(job);
    } else {
        success = timeline_run_decode_job(job);
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
timeline_log_has_required_fields(char const *line)
{
    if (strstr(line, "\"session_id\":") == NULL) {
        return 0;
    }
    if (strstr(line, "\"thread\":") == NULL) {
        return 0;
    }
    if (strstr(line, "\"worker\":") == NULL) {
        return 0;
    }
    if (strstr(line, "\"role\":") == NULL) {
        return 0;
    }
    if (strstr(line, "\"event\":") == NULL) {
        return 0;
    }
    if (strstr(line, "\"job\":") == NULL) {
        return 0;
    }
    return 1;
}

static int
timeline_parse_session_id(char const *line, unsigned int *session_id)
{
    char const *field;
    char *endptr;
    unsigned long value;

    if (line == NULL || session_id == NULL) {
        return 0;
    }
    field = strstr(line, "\"session_id\":");
    if (field == NULL) {
        return 0;
    }
    field += strlen("\"session_id\":");
    value = strtoul(field, &endptr, 10);
    if (endptr == (char *)field || value == 0UL) {
        return 0;
    }
    *session_id = (unsigned int)value;
    return 1;
}

static void
timeline_record_session(unsigned int *session_ids,
                        int *session_count,
                        unsigned int session_id)
{
    int index;

    for (index = 0; index < *session_count; ++index) {
        if (session_ids[index] == session_id) {
            return;
        }
    }
    if (*session_count < TIMELINE_MAX_SESSIONS) {
        session_ids[*session_count] = session_id;
        *session_count = *session_count + 1;
    }
}

static int
timeline_verify_jsonl(char const *path)
{
    FILE *file;
    unsigned int session_ids[TIMELINE_MAX_SESSIONS];
    unsigned int session_id;
    char line[1024];
    int line_count;
    int session_count;
    size_t line_len;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    file = sixel_compat_fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "timeline log was not written: %s\n", path);
        return 0;
    }

    line_count = 0;
    session_count = 0;
    memset(session_ids, 0, sizeof(session_ids));
    while (fgets(line, sizeof(line), file) != NULL) {
        line_len = strlen(line);
        if (line_len == 0u || line[line_len - 1u] != '\n') {
            fprintf(stderr, "timeline log contains a broken JSONL line\n");
            fclose(file);
            return 0;
        }
        if (!timeline_log_has_required_fields(line)) {
            fprintf(stderr, "timeline log line is missing required fields\n");
            fclose(file);
            return 0;
        }
        if (!timeline_parse_session_id(line, &session_id)) {
            fprintf(stderr, "timeline log line has invalid session_id\n");
            fclose(file);
            return 0;
        }
        timeline_record_session(session_ids, &session_count, session_id);
        line_count = line_count + 1;
    }
    fclose(file);

    if (line_count == 0) {
        fprintf(stderr, "timeline log did not receive any records\n");
        return 0;
    }
    if (session_count < 2) {
        fprintf(stderr, "timeline log has fewer than two sessions\n");
        return 0;
    }

    return 1;
}

static int
timeline_flush_writer(char const *path)
{
    SIXELSTATUS status;
    sixel_timeline_writer_t *writer;
    void *service;
    int success;

    writer = NULL;
    service = NULL;
    success = 0;

    if (path == NULL || path[0] == '\0') {
        return 1;
    }

    status = sixel_components_getservice("services/timeline-writer",
                                         &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    writer = (sixel_timeline_writer_t *)service;
    service = NULL;
    if (writer == NULL || writer->vtbl == NULL ||
        writer->vtbl->flush == NULL || writer->vtbl->unref == NULL) {
        goto end;
    }

    writer->vtbl->flush(writer);
    success = 1;

end:
    if (writer != NULL && writer->vtbl != NULL &&
        writer->vtbl->unref != NULL) {
        writer->vtbl->unref(writer);
    }
    return success;
}

static int
timeline_parallel_encode_decode(void)
{
    sixel_thread_t threads[TIMELINE_PARALLEL_WORKERS];
    timeline_parallel_job_t jobs[TIMELINE_PARALLEL_WORKERS];
    timeline_parallel_sync_t sync;
    char encode_input_path[4096];
    char decode_input_path[4096];
    char log_path[4096];
    char const *log_path_env;
    char const *output_path;
    int created;
    int index;
    int success;
    int sync_ready;

    created = 0;
    success = 0;
    sync_ready = 0;
    log_path_env = NULL;
    memset(threads, 0, sizeof(threads));
    memset(jobs, 0, sizeof(jobs));
    memset(&sync, 0, sizeof(sync));
    memset(encode_input_path, 0, sizeof(encode_input_path));
    memset(decode_input_path, 0, sizeof(decode_input_path));
    memset(log_path, 0, sizeof(log_path));

    if (!timeline_build_source_path(encode_input_path,
                                    sizeof(encode_input_path),
                                    "tests/data/inputs/small.ppm")) {
        goto end;
    }
    if (!timeline_build_source_path(
            decode_input_path,
            sizeof(decode_input_path),
            "tests/data/inputs/formats/small_gif_atkinson_static_reference.six"
        )) {
        goto end;
    }

    output_path = timeline_null_output_path();
    /*
     * MSVC compat getenv keeps an internal per-name cache.  The shared
     * timeline writer also reads SIXEL_LOG_PATH, so keep a private copy for
     * the verification path instead of retaining the returned pointer.
     */
    log_path_env = sixel_compat_getenv("SIXEL_LOG_PATH");
    if (log_path_env != NULL && log_path_env[0] != '\0' &&
        sixel_compat_strcpy(log_path, sizeof(log_path), log_path_env) < 0) {
        goto end;
    }

    if (SIXEL_FAILED(sixel_mutex_init(&sync.mutex))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_cond_init(&sync.cond))) {
        sixel_mutex_destroy(&sync.mutex);
        goto end;
    }
    sync_ready = 1;
    sync.worker_count = TIMELINE_PARALLEL_WORKERS;

    for (index = 0; index < TIMELINE_PARALLEL_WORKERS; ++index) {
        jobs[index].sync = &sync;
        jobs[index].encode_input_path = encode_input_path;
        jobs[index].decode_input_path = decode_input_path;
        jobs[index].output_path = output_path;
        jobs[index].kind = index % 2;
        jobs[index].index = index;
        if (SIXEL_FAILED(sixel_thread_create(&threads[index],
                                             timeline_parallel_worker,
                                             &jobs[index]))) {
            goto end;
        }
        created = created + 1;
    }

    sixel_mutex_lock(&sync.mutex);
    while (sync.ready_count < sync.worker_count) {
        sixel_cond_wait(&sync.cond, &sync.mutex);
    }
    sync.released = 1;
    sixel_cond_broadcast(&sync.cond);
    sixel_mutex_unlock(&sync.mutex);

    success = 1;

end:
    if (sync_ready != 0 && success == 0) {
        timeline_parallel_release(&sync);
    }
    for (index = 0; index < created; ++index) {
        sixel_thread_join(&threads[index]);
        if (threads[index].result != EXIT_SUCCESS) {
            success = 0;
        }
    }
    if (sync_ready != 0) {
        sixel_cond_destroy(&sync.cond);
        sixel_mutex_destroy(&sync.mutex);
    }
    if (success == 0) {
        return 0;
    }
    if (!timeline_flush_writer(log_path)) {
        return 0;
    }
    return 1;
}
#endif

int
test_timeline_0002_timeline_parallel_encode_decode(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if SIXEL_ENABLE_THREADS
    if (!timeline_parallel_encode_decode()) {
        fprintf(stderr, "timeline parallel encode/decode contract failed\n");
        return EXIT_FAILURE;
    }
#endif

    return EXIT_SUCCESS;
}

int
test_timeline_0002_timeline_parallel_encode_decode_verify(int argc,
                                                          char **argv)
{
    char log_path[4096];
    char const *log_path_source;

    log_path_source = NULL;
#if SIXEL_ENABLE_THREADS
    memset(log_path, 0, sizeof(log_path));
    if (argc >= 2 && argv != NULL && argv[1] != NULL &&
        argv[1][0] != '\0') {
        log_path_source = argv[1];
    } else {
        /*
         * Keep the fallback for direct developer runs and artifact triage.
         * The TAP test verifies in the producer process so MSVC shell/native
         * process handoff does not decide CI success.
         */
        log_path_source = sixel_compat_getenv("SIXEL_LOG_PATH");
    }
    if (log_path_source == NULL || log_path_source[0] == '\0' ||
        sixel_compat_strcpy(log_path,
                            sizeof(log_path),
                            log_path_source) < 0 ||
        !timeline_verify_jsonl(log_path)) {
        fprintf(stderr, "timeline parallel JSONL verification failed\n");
        return EXIT_FAILURE;
    }
#else
    (void)log_path;
    (void)log_path_source;
    (void)argc;
    (void)argv;
#endif

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
