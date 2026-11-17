/* SPDX-License-Identifier: MIT */
#ifndef PNG_HDR_COMPOSITION_COMMON_H
#define PNG_HDR_COMPOSITION_COMMON_H
/* Backends: builtin PNG, libpng PNG, builtin embedded BMP PNG.
 * Scenarios: alpha policy/background, bKGD/manual source, partial APNG. */
int png_hdr_composition_test(int backend, int scenario, int option);
#endif
