/*
 * Shared helpers for precision/float32 environment handling.
 */
#ifndef LIBSIXEL_PRECISION_H
#define LIBSIXEL_PRECISION_H

#ifdef __cplusplus
extern "C" {
#endif

#define SIXEL_FLOAT32_ENVVAR "SIXEL_FLOAT32_DITHER"

int sixel_precision_env_wants_float32(void);
void sixel_precision_reset_float32_cache(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PRECISION_H */
