/*
 * SPDX-License-Identifier: MIT
 *
 * Automatic output intent belongs to img2sixel, not the encoder library.
 * Resolve only advertised fixed input spaces; never guess a display ICC.
 */
#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "terminal-color-job.h"

#include "compat.h"

/*
 * One converter-owned typed row binds parsing, environment, bounds and
 * defaults. The staticcheck registry audit also reads this public metadata.
 * Neither this table nor automatic terminal intent enters the library.
 */
typedef struct img2sixel_color_key {
    char const *base;
    char const *name;
    char short_name;
    char const *environment;
    size_t value_offset;
    size_t override_offset;
    int minimum;
    int maximum;
    int initial;
} img2sixel_color_key_t;

#if defined(__GNUC__) || defined(__clang__)
# define IMG2SIXEL_COLOR_INT_OFFSET(field_) \
    (offsetof(img2sixel_color_job_t, field_) + \
     0u * sizeof(char[__builtin_types_compatible_p( \
         __typeof__(((img2sixel_color_job_t *)0)->field_), int) ? 1 : -1]))
#else
# define IMG2SIXEL_COLOR_INT_OFFSET(field_) \
    (offsetof(img2sixel_color_job_t, field_) + \
     0u * sizeof(char[sizeof(((img2sixel_color_job_t *)0)->field_) == \
                     sizeof(int) ? 1 : -1]))
#endif

#define IMG2SIXEL_REGISTRY_COLOR_INT( \
    base_, name_, short_, env_, field_, override_, min_, max_, initial_) \
    { (base_), (name_), (short_), (env_), \
      IMG2SIXEL_COLOR_INT_OFFSET(field_), \
      IMG2SIXEL_COLOR_INT_OFFSET(override_), (min_), (max_), (initial_) }

static img2sixel_color_key_t const g_img2sixel_color_policy_keys[] = {
    IMG2SIXEL_REGISTRY_COLOR_INT(
        "auto", "timeout_ms", 'T', "SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS",
        timeout_ms, timeout_override, 1, 10000, 200),
};

static int *
img2sixel_color_job_field(img2sixel_color_job_t *job, size_t offset)
{
    return (int *)((unsigned char *)job + offset);
}

static SIXELSTATUS
img2sixel_color_job_error(char const *message)
{
    sixel_helper_set_additional_message(message);
    return SIXEL_RUNTIME_ERROR;
}

static SIXELSTATUS
img2sixel_color_job_argument(char const *message)
{
    sixel_helper_set_additional_message(message);
    return SIXEL_BAD_ARGUMENT;
}

static int
img2sixel_color_job_integer(char const *text, size_t size,
                           img2sixel_color_key_t const *key, int *value)
{
    size_t i;
    int parsed;
    int digit;

    parsed = 0;
    if (size == 0u) {
        return 0;
    }
    for (i = 0u; i < size; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        digit = text[i] - '0';
        if (parsed > (key->maximum - digit) / 10) {
            return 0;
        }
        parsed = parsed * 10 + digit;
    }
    if (parsed < key->minimum || parsed > key->maximum) {
        return 0;
    }
    *value = parsed;
    return 1;
}

void
img2sixel_color_job_init(img2sixel_color_job_t *job)
{
    memset(job, 0, sizeof(*job));
    *img2sixel_color_job_field(
        job, g_img2sixel_color_policy_keys[0].value_offset) =
        g_img2sixel_color_policy_keys[0].initial;
    job->stdout_only = 1;
}

SIXELSTATUS
img2sixel_color_job_option(img2sixel_color_job_t *job,
                           sixel_encoder_t *encoder, char const *argument)
{
    img2sixel_color_key_t const *key;
    char const *suffix;
    char const *end;
    char const *value;
    size_t base_size;
    size_t i;
    int automatic;
    int timeout;
    int override;
    SIXELSTATUS status;

    key = &g_img2sixel_color_policy_keys[0];
    if (argument == NULL) {
        return img2sixel_color_job_argument(
            "output-colorspace requires an argument.");
    }
    suffix = strchr(argument, ':');
    base_size = suffix == NULL ? strlen(argument)
                              : (size_t)(suffix - argument);
    automatic = base_size > 0u && base_size <= strlen(key->base);
    for (i = 0u; automatic && i < base_size; ++i) {
        automatic = tolower((unsigned char)argument[i]) == key->base[i];
    }
    timeout = key->initial;
    override = 0;
    while (automatic && suffix != NULL) {
        ++suffix;
        end = strchr(suffix, ':');
        if (end == NULL) {
            end = suffix + strlen(suffix);
        }
        value = NULL;
        if ((size_t)(end - suffix) > strlen(key->name) &&
            strncmp(suffix, key->name, strlen(key->name)) == 0 &&
            suffix[strlen(key->name)] == '=') {
            value = suffix + strlen(key->name) + 1u;
        } else if (suffix < end && suffix[0] == key->short_name) {
            value = suffix + 1;
        }
        if (value == NULL ||
            !img2sixel_color_job_integer(value, (size_t)(end - value),
                                         key, &timeout)) {
            return img2sixel_color_job_argument(
                "output-colorspace auto accepts timeout_ms=N or TN "
                "with decimal N in 1..10000.");
        }
        override = 1;
        suffix = *end == ':' ? end : NULL;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT_COLORSPACE,
                                   automatic ? "gamma" : argument);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    job->automatic = automatic;
    *img2sixel_color_job_field(job, key->value_offset) = timeout;
    *img2sixel_color_job_field(job, key->override_offset) = override;
    job->requested = 1;
    return SIXEL_OK;
}

void
img2sixel_color_job_outfile(img2sixel_color_job_t *job,
                          char const *argument)
{
    if (argument == NULL || strcmp(argument, "-") != 0) {
        job->stdout_only = 0;
    }
}

SIXELSTATUS
img2sixel_color_job_prepare(img2sixel_color_job_t *job,
                            sixel_encoder_t *encoder, int output_fd,
                            img2sixel_color_cancel_t cancel, void *context)
{
    img2sixel_color_key_t const *key;
    char const *value;
    SIXELSTATUS status;

    job->output_fd = output_fd;
    job->cancel = cancel;
    job->cancel_context = context;
    if (!job->requested) {
        value = img2sixel_compat_getenv("SIXEL_OUTPUT_COLORSPACE");
        if (value != NULL && value[0] != '\0') {
            /* The base environment is a choice, not a structured CLI token.
             * Its separate timeout environment owns that subordinate value. */
            if (strchr(value, ':') != NULL) {
                return img2sixel_color_job_argument(
                    "SIXEL_OUTPUT_COLORSPACE requires a color-space name; "
                    "use SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS for timeout.");
            }
            status = img2sixel_color_job_option(job, encoder, value);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
    }
    /* T=1 describes ordinary SIXEL. DRCS has a separate T=2 policy;
     * applying a T=1 result to glyph definitions would not establish their
     * interpretation, even when auto resolves to gamma. */
    if (job->automatic && job->drcs) {
        return img2sixel_color_job_argument(
            "output-colorspace auto does not support DRCS output.");
    }
    key = &g_img2sixel_color_policy_keys[0];
    if (job->automatic &&
        !*img2sixel_color_job_field(job, key->override_offset)) {
        value = img2sixel_compat_getenv(key->environment);
        if (value != NULL && value[0] != '\0' &&
            !img2sixel_color_job_integer(value, strlen(value), key,
                                         img2sixel_color_job_field(
                                             job, key->value_offset))) {
            return img2sixel_color_job_argument(
                "SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS requires "
                "a decimal integer in 1..10000.");
        }
    }
    return SIXEL_OK;
}

static int
img2sixel_color_job_cancelled(img2sixel_color_job_t const *job)
{
    return job->cancel != NULL && job->cancel(job->cancel_context);
}

static img2sixel_color_io_t
img2sixel_color_job_query(img2sixel_color_job_t *job,
                          img2sixel_color_query_t query,
                          img2sixel_color_exchange_t *reply, int *unfinished)
{
    img2sixel_color_io_t result;
    size_t request_size;

    request_size = query == IMG2SIXEL_COLOR_CAPABILITY ? 7u : 10u;
    result = img2sixel_color_tty_query(job->tty, query,
                                      job->timeout_ms, reply);
    if (reply->request_written > 0u &&
        reply->request_written < request_size) {
        *unfinished = 1;
    }
    return result;
}

static img2sixel_color_io_t
img2sixel_color_job_policy(img2sixel_color_job_t *job, int policy,
                           int recovery, int *unfinished)
{
    char sequence[] = "\033[1;0./p";
    size_t written;
    img2sixel_color_io_t result;

    sequence[4] = (char)('0' + policy);
    result = img2sixel_color_tty_write(job->tty, sequence,
                                      sizeof(sequence) - 1u,
                                      job->timeout_ms, recovery, &written);
    if (written > 0u && written < sizeof(sequence) - 1u) {
        *unfinished = 1;
    }
    return result;
}

SIXELSTATUS
img2sixel_color_job_encode(img2sixel_color_job_t *job,
                           sixel_encoder_t *encoder, char const *input)
{
    img2sixel_color_exchange_t reply;
    img2sixel_color_io_t io;
    SIXELSTATUS status;
    char const *failure;
    char const *detail;
    char encode_detail[2048];
    char combined[2304];
    size_t written;
    int active;
    int saved;
    int changed;
    int unfinished;
    int correlated;
    int recovery_failed;
    int encoding_failed;

    if (!job->automatic) {
        return sixel_encoder_encode(encoder, input);
    }
    if (img2sixel_color_job_cancelled(job)) {
        img2sixel_color_tty_abandon(job->tty);
        sixel_helper_set_additional_message(
            "output-colorspace auto was interrupted.");
        return SIXEL_INTERRUPTED;
    }
    /* File/pipe output is interchange. The same branch includes any outfile
     * override: stderr's terminal cannot identify the eventual consumer. */
    if (!job->stdout_only || (job->probed && job->selected_policy == 0)) {
        return sixel_encoder_encode(encoder, input);
    }
    active = 0;
    saved = -1;
    changed = 0;
    unfinished = 0;
    correlated = 1;
    recovery_failed = 0;
    encoding_failed = 0;
    encode_detail[0] = '\0';
    failure = NULL;
    status = SIXEL_OK;
    if (job->tty == NULL) {
        io = img2sixel_color_tty_open(&job->tty, job->output_fd,
                                     job->cancel, job->cancel_context);
        if (io == IMG2SIXEL_COLOR_IO_UNAVAILABLE) {
            job->probed = 1;
            job->selected_policy = 0;
            return sixel_encoder_encode(encoder, input);
        }
        if (io != IMG2SIXEL_COLOR_IO_OK) {
            return img2sixel_color_job_error(
                "output-colorspace auto could not open its output terminal.");
        }
    }
    io = img2sixel_color_tty_begin(job->tty);
    if (io != IMG2SIXEL_COLOR_IO_OK) {
        return img2sixel_color_job_error(
            "output-colorspace auto lost its foreground output terminal.");
    }
    active = 1;
    if (!job->probed) {
        io = img2sixel_color_job_query(job, IMG2SIXEL_COLOR_CAPABILITY,
                                       &reply, &unfinished);
        if (io == IMG2SIXEL_COLOR_IO_SILENT ||
            (io == IMG2SIXEL_COLOR_IO_OK &&
             reply.parser.result == IMG2SIXEL_COLOR_UNAVAILABLE)) {
            /* This is unconfirmed legacy output, not detection of sRGB. */
            job->probed = 1;
            job->selected_policy = 0;
            goto encode;
        }
        if (io != IMG2SIXEL_COLOR_IO_OK ||
            (reply.parser.result != IMG2SIXEL_COLOR_AVAILABLE &&
             reply.parser.result != IMG2SIXEL_COLOR_UNSUPPORTED)) {
            failure = "output-colorspace auto capability query failed.";
            correlated = 0;
            goto cleanup;
        }
        job->selected_policy = reply.parser.definition == 4u &&
            reply.parser.s2 == 1u ? 2 : 1;
        job->probed = 1;
    }
    io = img2sixel_color_job_query(job, IMG2SIXEL_COLOR_POLICY,
                                   &reply, &unfinished);
    if (io != IMG2SIXEL_COLOR_IO_OK ||
        reply.parser.result != IMG2SIXEL_COLOR_SAVED_POLICY) {
        failure = "output-colorspace auto could not save terminal policy.";
        correlated = 0;
        goto cleanup;
    }
    saved = (int)reply.parser.policy;
    changed = 1;
    io = img2sixel_color_job_policy(job, job->selected_policy, 0,
                                    &unfinished);
    if (io != IMG2SIXEL_COLOR_IO_OK) {
        failure = "output-colorspace auto could not set terminal policy.";
        correlated = 0;
        goto cleanup;
    }
    io = img2sixel_color_job_query(job, IMG2SIXEL_COLOR_POLICY,
                                   &reply, &unfinished);
    if (io != IMG2SIXEL_COLOR_IO_OK ||
        reply.parser.result != IMG2SIXEL_COLOR_SAVED_POLICY ||
        reply.parser.policy != (unsigned long)job->selected_policy) {
        failure = "output-colorspace auto policy confirmation failed.";
        correlated = 0;
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT_COLORSPACE,
                                   job->selected_policy == 2
                                       ? "display-p3" : "gamma");
    if (SIXEL_FAILED(status)) {
        failure = "output-colorspace auto could not apply resolved intent.";
        goto cleanup;
    }

encode:
    io = img2sixel_color_tty_end(job->tty);
    if (io != IMG2SIXEL_COLOR_IO_OK) {
        failure = "output-colorspace auto could not release terminal mode.";
        correlated = 0;
        goto cleanup;
    }
    active = 0;
    status = sixel_encoder_encode(encoder, input);
    if (status != SIXEL_OK || img2sixel_color_job_cancelled(job)) {
        /* Preserve the original encoder diagnostic before cleanup. Its
         * interrupted status is meaningful even though SIXEL_SUCCEEDED is
         * true for it. Never retry the image with a different color space. */
        if (status == SIXEL_OK) {
            status = SIXEL_INTERRUPTED;
        }
        encoding_failed = 1;
        detail = sixel_helper_get_additional_message();
        /* Bound this snapshot explicitly; cleanup adds its own diagnostic
         * in combined, so the original message may need truncation. */
        (void)snprintf(encode_detail, sizeof(encode_detail), "%.*s",
                       (int)sizeof(encode_detail) - 1,
                       detail == NULL ? "" : detail);
        unfinished = 1;
        if (status == SIXEL_INTERRUPTED ||
            img2sixel_color_job_cancelled(job)) {
            correlated = 0;
        }
    }

cleanup:
    if (!correlated) {
        img2sixel_color_tty_abandon(job->tty);
    }
    if ((changed || unfinished) && !active) {
        io = img2sixel_color_tty_begin(job->tty);
        if (io == IMG2SIXEL_COLOR_IO_OK) {
            active = 1;
        } else {
            recovery_failed = 1;
        }
    }
    if (active && unfinished) {
        io = img2sixel_color_tty_write(job->tty, "\030\033\\", 3u,
                                      job->timeout_ms, 1, &written);
        if (io != IMG2SIXEL_COLOR_IO_OK) {
            recovery_failed = 1;
            correlated = 0;
        }
    }
    if (active && changed) {
        unfinished = 0;
        io = img2sixel_color_job_policy(job, saved, 1, &unfinished);
        if (io != IMG2SIXEL_COLOR_IO_OK) {
            recovery_failed = 1;
        } else if (correlated && !img2sixel_color_job_cancelled(job)) {
            io = img2sixel_color_job_query(job, IMG2SIXEL_COLOR_POLICY,
                                           &reply, &unfinished);
            if (io != IMG2SIXEL_COLOR_IO_OK ||
                reply.parser.result != IMG2SIXEL_COLOR_SAVED_POLICY ||
                reply.parser.policy != (unsigned long)saved) {
                img2sixel_color_tty_abandon(job->tty);
                recovery_failed = 1;
            }
        }
    }
    if (active && recovery_failed && unfinished) {
        /* A failed restoration write or its verification request can itself
         * leave a control open. End framing without issuing another query. */
        (void)img2sixel_color_tty_write(job->tty, "\030\033\\", 3u,
                                       job->timeout_ms, 1, &written);
    }
    if (active &&
        img2sixel_color_tty_end(job->tty) != IMG2SIXEL_COLOR_IO_OK) {
        recovery_failed = 1;
    }
    /* Cancellation may arrive during the recovery write or final tcsetattr,
     * after encoding already returned OK. Latch it after releasing termios
     * as well, so write-only restoration never becomes verified success and
     * another input cannot start a new exchange on this context. */
    if (img2sixel_color_job_cancelled(job)) {
        img2sixel_color_tty_abandon(job->tty);
        if (status == SIXEL_OK && failure == NULL) {
            status = SIXEL_INTERRUPTED;
            encoding_failed = 1;
            (void)snprintf(encode_detail, sizeof(encode_detail), "%s",
                "output-colorspace auto was interrupted during cleanup.");
        }
    }
    if (encoding_failed) {
        (void)snprintf(combined, sizeof(combined), "%s%s", encode_detail,
                       recovery_failed
                       ? " Terminal color policy/settings recovery failed."
                       : "");
        sixel_helper_set_additional_message(combined);
        return status;
    }
    if (failure != NULL || recovery_failed) {
        (void)snprintf(combined, sizeof(combined), "%s%s",
                       failure == NULL ? "" : failure,
                       recovery_failed
                       ? " Terminal color policy/settings recovery failed."
                       : "");
        return img2sixel_color_job_error(combined);
    }
    return status;
}

SIXELSTATUS
img2sixel_color_job_close(img2sixel_color_job_t *job)
{
    img2sixel_color_io_t io;

    io = img2sixel_color_tty_close(job->tty);
    job->tty = NULL;
    return io == IMG2SIXEL_COLOR_IO_OK ? SIXEL_OK : SIXEL_RUNTIME_ERROR;
}
