/* SPDX-License-Identifier: MIT */
/* Compile the actual controller under isolated names. Deterministic callback
 * boundaries complement the real executable/PTY signal cases in 0039. */
#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include "converters/terminal-color-job.h"

static int cjf_inject;
static int cjf_bad_encode;
static int cjf_cancelled;
static int cjf_queries;
static int cjf_abandoned;
static int cjf_encodes;
static int cjf_ends;
static int cjf_active;
static char cjf_message[4096];

/* A small independent receiver model, not the production response parser.
 * CSI bytes inside an unterminated DCS are payload and cannot set policy.
 * Only CAN or ST returns this receiver to a state that accepts restoration.
 * This models an interrupted encoder boundary; it is not a real signal test.
 */
static int cjf_receive_state;
static int cjf_receive_policy;
static int cjf_receive_restored;
static size_t cjf_receive_size;
static char cjf_receive_csi[32];

static void
cjf_receive(char const *bytes, size_t size)
{
    size_t i;
    unsigned char byte;

    for (i = 0u; i < size; ++i) {
        byte = (unsigned char)bytes[i];
        if (byte == 0x18u || byte == 0x1au) {
            cjf_receive_state = 0;
        } else if (cjf_receive_state == 2) {
            if (byte == 0x1bu) {
                cjf_receive_state = 3;
            }
        } else if (cjf_receive_state == 3) {
            cjf_receive_state = byte == '\\' ? 0 : 2;
        } else if (cjf_receive_state == 1) {
            cjf_receive_state = byte == 'P' ? 2 : byte == '[' ? 4 : 0;
            cjf_receive_size = 0u;
        } else if (cjf_receive_state == 4) {
            if (cjf_receive_size + 1u < sizeof(cjf_receive_csi)) {
                cjf_receive_csi[cjf_receive_size++] = (char)byte;
                cjf_receive_csi[cjf_receive_size] = '\0';
            }
            if (byte >= 0x40u && byte <= 0x7eu) {
                if (strcmp(cjf_receive_csi, "1;2./p") == 0) {
                    cjf_receive_policy = 2;
                } else if (strcmp(cjf_receive_csi, "1;0./p") == 0) {
                    cjf_receive_policy = 0;
                    ++cjf_receive_restored;
                }
                cjf_receive_state = 0;
            }
        } else if (byte == 0x1bu) {
            cjf_receive_state = 1;
        }
    }
}

static char const *
cjf_getenv(char const *name)
{
    (void)name;
    return NULL;
}

static void
cjf_set_message(char const *message)
{
    snprintf(cjf_message, sizeof(cjf_message), "%s", message);
}

static char const *
cjf_get_message(void)
{
    return cjf_message;
}

static SIXELSTATUS
cjf_setopt(sixel_encoder_t *encoder, int option, char const *value)
{
    (void)encoder;
    return option == SIXEL_OPTFLAG_OUTPUT_COLORSPACE &&
        (strcmp(value, "gamma") == 0 || strcmp(value, "display-p3") == 0)
        ? SIXEL_OK : SIXEL_BAD_ARGUMENT;
}

static SIXELSTATUS
cjf_encode(sixel_encoder_t *encoder, char const *input)
{
    (void)encoder;
    (void)input;
    ++cjf_encodes;
    if (cjf_active) {
        return SIXEL_RUNTIME_ERROR;
    }
    if (cjf_inject == 3) {
        cjf_receive("\033Pq#0;2;5", sizeof("\033Pq#0;2;5") - 1u);
        if (cjf_receive_state != 2 || cjf_receive_policy != 2) {
            return SIXEL_LOGIC_ERROR;
        }
        cjf_cancelled = 1;
        return SIXEL_INTERRUPTED;
    }
    if (cjf_bad_encode) {
        cjf_set_message("original encoder failure");
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

static int
cjf_cancel(void *context)
{
    (void)context;
    return cjf_cancelled;
}

static img2sixel_color_io_t
cjf_open(img2sixel_color_tty_t **tty, int fd,
         img2sixel_color_cancel_t cancel, void *context)
{
    (void)fd;
    (void)cancel;
    (void)context;
    *tty = (img2sixel_color_tty_t *)&cjf_queries;
    return IMG2SIXEL_COLOR_IO_OK;
}

static img2sixel_color_io_t
cjf_begin(img2sixel_color_tty_t *tty)
{
    (void)tty;
    cjf_active = 1;
    return IMG2SIXEL_COLOR_IO_OK;
}

static img2sixel_color_io_t
cjf_end(img2sixel_color_tty_t *tty)
{
    (void)tty;
    cjf_active = 0;
    if (++cjf_ends == 2 && cjf_inject == 2) {
        cjf_cancelled = 1;
    }
    return IMG2SIXEL_COLOR_IO_OK;
}

static img2sixel_color_io_t
cjf_close(img2sixel_color_tty_t *tty)
{
    (void)tty;
    return IMG2SIXEL_COLOR_IO_OK;
}

static void
cjf_abandon(img2sixel_color_tty_t *tty)
{
    (void)tty;
    cjf_abandoned = 1;
}

static img2sixel_color_io_t
cjf_query(img2sixel_color_tty_t *tty, img2sixel_color_query_t query,
          int timeout, img2sixel_color_exchange_t *reply)
{
    (void)tty;
    memset(reply, 0, sizeof(*reply));
    ++cjf_queries;
    reply->request_written = query == IMG2SIXEL_COLOR_CAPABILITY ? 7u : 10u;
    reply->parser.result = query == IMG2SIXEL_COLOR_CAPABILITY
        ? IMG2SIXEL_COLOR_AVAILABLE : IMG2SIXEL_COLOR_SAVED_POLICY;
    reply->parser.definition = 4u;
    reply->parser.s2 = 1u;
    reply->parser.policy = cjf_queries == 3 ? 2u : 0u;
    return cjf_active && !cjf_abandoned && timeout == 200
        ? IMG2SIXEL_COLOR_IO_OK : IMG2SIXEL_COLOR_IO_ERROR;
}

static img2sixel_color_io_t
cjf_write(img2sixel_color_tty_t *tty, char const *bytes, size_t size,
          int timeout, int recovery, size_t *written)
{
    (void)tty;
    *written = size;
    if (cjf_inject == 3) {
        cjf_receive(bytes, size);
    }
    if (recovery && size == 8u && bytes[1] == '[' && cjf_inject == 1) {
        cjf_cancelled = 1;
    }
    return cjf_active && timeout == 200
        ? IMG2SIXEL_COLOR_IO_OK : IMG2SIXEL_COLOR_IO_ERROR;
}

#define img2sixel_compat_getenv cjf_getenv
#define sixel_helper_set_additional_message cjf_set_message
#define sixel_helper_get_additional_message cjf_get_message
#define sixel_encoder_setopt cjf_setopt
#define sixel_encoder_encode cjf_encode
#define img2sixel_color_tty_open cjf_open
#define img2sixel_color_tty_begin cjf_begin
#define img2sixel_color_tty_end cjf_end
#define img2sixel_color_tty_close cjf_close
#define img2sixel_color_tty_abandon cjf_abandon
#define img2sixel_color_tty_query cjf_query
#define img2sixel_color_tty_write cjf_write
#define g_img2sixel_color_policy_keys cjf_copy_g_policy_keys
#define img2sixel_color_job_argument cjf_copy_argument
#define img2sixel_color_job_cancelled cjf_copy_cancelled
#define img2sixel_color_job_close cjf_copy_close
#define img2sixel_color_job_encode cjf_copy_encode
#define img2sixel_color_job_error cjf_copy_error
#define img2sixel_color_job_field cjf_copy_field
#define img2sixel_color_job_init cjf_copy_init
#define img2sixel_color_job_integer cjf_copy_integer
#define img2sixel_color_job_option cjf_copy_option
#define img2sixel_color_job_outfile cjf_copy_outfile
#define img2sixel_color_job_policy cjf_copy_policy
#define img2sixel_color_job_prepare cjf_copy_prepare
#define img2sixel_color_job_query cjf_copy_query
#define img2sixel_color_key cjf_copy_key
#define img2sixel_color_key_t cjf_copy_key_t
#include "converters/terminal-color-job.c"
#undef g_img2sixel_color_policy_keys
#undef img2sixel_color_job_argument
#undef img2sixel_color_job_cancelled
#undef img2sixel_color_job_close
#undef img2sixel_color_job_encode
#undef img2sixel_color_job_error
#undef img2sixel_color_job_field
#undef img2sixel_color_job_init
#undef img2sixel_color_job_integer
#undef img2sixel_color_job_option
#undef img2sixel_color_job_outfile
#undef img2sixel_color_job_policy
#undef img2sixel_color_job_prepare
#undef img2sixel_color_job_query
#undef img2sixel_color_key
#undef img2sixel_color_key_t
#undef img2sixel_compat_getenv
#undef sixel_helper_set_additional_message
#undef sixel_helper_get_additional_message
#undef sixel_encoder_setopt
#undef sixel_encoder_encode
#undef img2sixel_color_tty_open
#undef img2sixel_color_tty_begin
#undef img2sixel_color_tty_end
#undef img2sixel_color_tty_close
#undef img2sixel_color_tty_abandon
#undef img2sixel_color_tty_query
#undef img2sixel_color_tty_write

int
test_cli_0040_color_job(int argc, char **argv)
{
    img2sixel_color_job_t job;
    SIXELSTATUS status;
    SIXELSTATUS expected;
    int queries;
    int encodes;
    sixel_encoder_t *encoder;

    if (argc != 2) {
        return 1;
    }
    if (strcmp(argv[1], "library-auto") == 0) {
        encoder = NULL;
        if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) {
            return 1;
        }
        status = sixel_encoder_setopt(encoder,
                                       SIXEL_OPTFLAG_OUTPUT_COLORSPACE,
                                       "auto");
        sixel_encoder_unref(encoder);
        return status != SIXEL_BAD_ARGUMENT;
    }
    if (strcmp(argv[1], "control-success") != 0 &&
        strcmp(argv[1], "control-error") != 0 &&
        strcmp(argv[1], "restore-success") != 0 &&
        strcmp(argv[1], "restore-error") != 0 &&
        strcmp(argv[1], "end-success") != 0 &&
        strcmp(argv[1], "end-error") != 0 &&
        strcmp(argv[1], "partial-encode") != 0) {
        return 1;
    }
    cjf_inject = strncmp(argv[1], "restore-", 8u) == 0 ? 1
        : strncmp(argv[1], "end-", 4u) == 0 ? 2
        : strcmp(argv[1], "partial-encode") == 0 ? 3 : 0;
    cjf_bad_encode = strstr(argv[1], "error") != NULL;
    cjf_cancelled = 0;
    cjf_queries = 0;
    cjf_abandoned = 0;
    cjf_encodes = 0;
    cjf_ends = 0;
    cjf_active = 0;
    cjf_receive_state = 0;
    cjf_receive_policy = 0;
    cjf_receive_restored = 0;
    cjf_message[0] = '\0';
    cjf_copy_init(&job);
    if (cjf_copy_option(&job, NULL, "auto") != SIXEL_OK ||
        cjf_copy_prepare(&job, NULL, 1, cjf_cancel, NULL) != SIXEL_OK) {
        return 1;
    }
    status = cjf_copy_encode(&job, NULL, "input");
    expected = cjf_bad_encode ? SIXEL_BAD_ARGUMENT
        : cjf_inject ? SIXEL_INTERRUPTED : SIXEL_OK;
    if (status != expected || cjf_encodes != 1 || cjf_active ||
        cjf_queries != (cjf_inject == 1 || cjf_inject == 3 ? 3 : 4) ||
        cjf_abandoned != (cjf_inject != 0) ||
        (cjf_bad_encode &&
         strcmp(cjf_message, "original encoder failure") != 0)) {
        fprintf(stderr, "CLI color job: cleanup result or diagnostic\n");
        return 1;
    }
    if (cjf_inject == 3 && (cjf_receive_state != 0 ||
        cjf_receive_policy != 0 || cjf_receive_restored != 1)) {
        fprintf(stderr, "CLI color job: receiver did not restore policy\n");
        return 1;
    }
    queries = cjf_queries;
    encodes = cjf_encodes;
    if (cjf_inject &&
        (cjf_copy_encode(&job, NULL, "next") != SIXEL_INTERRUPTED ||
         cjf_queries != queries || cjf_encodes != encodes)) {
        fprintf(stderr, "CLI color job: input dispatched after cancellation\n");
        return 1;
    }
    return cjf_copy_close(&job) != SIXEL_OK;
}
