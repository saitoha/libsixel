#!/bin/sh
# TAP test covering forced GPU bluenoise with transparent keep accumulation.
#
# alpha-policy=keep with transparent-offset emits P2=1 padding without
# needing a 6delta retained RGB plane.  FORCE mode must still be able to
# exercise the GPU path for terminal video clients that prefer full-paint GPU
# output over CPU fallback.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0;
}

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 16 --lookup-policy=none -o /dev/null "${probe_image}" || {
    printf "1..0 # SKIP forced GPU palette apply is unavailable\n";
    exit 0;
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

helper_src="${ARTIFACT_LOCAL_DIR}/gpu-force-keep-offset.c"
helper_bin="${ARTIFACT_LOCAL_DIR}/gpu-force-keep-offset${SIXEL_BIN_EXT-}"
link_libdir="${LIBSIXEL_LIBDIR-${TOP_BUILDDIR}/src/.libs}"

test -d "${link_libdir}" || link_libdir="${TOP_BUILDDIR}/src"

runtime_dyld_path="${link_libdir}"
runtime_ld_path="${link_libdir}"

test -z "${DYLD_LIBRARY_PATH+x}" || {
    runtime_dyld_path="${runtime_dyld_path}:${DYLD_LIBRARY_PATH}"
}
test -z "${LD_LIBRARY_PATH+x}" || {
    runtime_ld_path="${runtime_ld_path}:${LD_LIBRARY_PATH}"
}

cat >"${helper_src}" <<'EOF_C'
#include <sixel.h>

#include <stdio.h>
#include <stdlib.h>

#define TEST_WIDTH 96
#define TEST_HEIGHT 72
#define TEST_PIXEL_COUNT (TEST_WIDTH * TEST_HEIGHT)
#define TEST_BYTES (TEST_PIXEL_COUNT * 3)

static int
test_write(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    if (size < 0) {
        return 1;
    }
    return 0;
}

static void
fill_pixels(unsigned char *pixels, int frame_no)
{
    int x;
    int y;
    size_t offset;

    x = 0;
    y = 0;
    offset = 0u;
    for (y = 0; y < TEST_HEIGHT; ++y) {
        for (x = 0; x < TEST_WIDTH; ++x) {
            offset = ((size_t)y * (size_t)TEST_WIDTH + (size_t)x) * 3u;
            pixels[offset + 0u] =
                (unsigned char)((x * 5 + y * 3 + frame_no * 7) & 0xff);
            pixels[offset + 1u] =
                (unsigned char)((x * 2 + y * 9 + frame_no * 5) & 0xff);
            pixels[offset + 2u] =
                (unsigned char)((x * 11 + y + frame_no * 13) & 0xff);
        }
    }
}

static SIXELSTATUS
encode_frame(sixel_encoder_t *encoder, unsigned char *pixels, int frame_no)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    sixel_output_t *output;

    status = SIXEL_FALSE;
    frame = NULL;
    output = NULL;
    if (encoder == NULL || pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame,
                                       pixels,
                                       TEST_WIDTH,
                                       TEST_HEIGHT,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       (-1));
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_frame_set_multiframe(frame, frame_no > 0 ? 1 : 0);

    status = sixel_output_new(&output, test_write, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);

end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    return status;
}

int
main(void)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    unsigned char *pixels;
    int frame_no;
    int ok;

    status = SIXEL_FALSE;
    encoder = NULL;
    pixels = NULL;
    frame_no = 0;
    ok = 0;

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "encoder allocation failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "256");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "colors option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
                                  "bluenoise");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "diffusion option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY, "none");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "lookup option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, "force");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "gpu option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_ALPHA_POLICY,
                                  "keep");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "alpha policy option failed: %04x\n", status);
        goto end;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_TRANSPARENT_OFFSET,
                                  "15,35");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "transparent offset option failed: %04x\n", status);
        goto end;
    }

    pixels = (unsigned char *)malloc(TEST_BYTES);
    if (pixels == NULL) {
        fprintf(stderr, "pixel allocation failed\n");
        goto end;
    }

    for (frame_no = 0; frame_no < 3; ++frame_no) {
        fill_pixels(pixels, frame_no);
        status = encode_frame(encoder, pixels, frame_no);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "frame %d encode failed: %04x\n",
                    frame_no,
                    status);
            goto end;
        }
    }

    ok = 1;

end:
    free(pixels);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return ok != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
EOF_C

"${CC:-cc}" \
        -I"${TOP_BUILDDIR}" \
        -I"${TOP_BUILDDIR}/include" \
        -I"${TOP_SRCDIR}/include" \
        -o "${helper_bin}" \
        "${helper_src}" \
        -L"${link_libdir}" \
        -lsixel \
        "-Wl,-rpath,${link_libdir}" || {
    echo "not ok" 1 - "forced GPU keep-offset helper build failed"
    exit 0
}

SIXEL_THREADS=1 \
SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
DYLD_LIBRARY_PATH="${runtime_dyld_path}" \
LD_LIBRARY_PATH="${runtime_ld_path}" \
${SIXEL_RUNTIME-} "${helper_bin}" >/dev/null || {
    echo "not ok" 1 - "forced GPU bluenoise keep-offset encode failed"
    exit 0
}

echo "ok" 1 - "forced GPU bluenoise accepts keep-offset accumulation"
exit 0
