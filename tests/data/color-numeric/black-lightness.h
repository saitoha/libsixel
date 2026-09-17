#ifndef COLOR_FIXTURE_BLACK_LIGHTNESS
#define COLOR_FIXTURE_BLACK_LIGHTNESS
/* Generated cancellation bounds and finite libm probes. */
static double const color_black_l_bounds[2] = {
    8.8817847263968478e-17,
    1.0398762171365921e-16,
};
static struct color_black_log_probe {
    uint64_t bits;
    double lo;
    double hi;
} const color_black_log_probes[] = {
    { UINT64_C(0x0000000000000000),
      0, 0 },
    { UINT64_C(0x8000000000000000),
      0, 0 },
    { UINT64_C(0xbc2d7dbf487fcb92),
      -7.9936362704872281e-19, -7.9935752841150257e-19 },
    { UINT64_C(0x3c2d7dbf487fcb92),
      7.9935752841150257e-19, 7.9936362704872281e-19 },
    { UINT64_C(0x3c826e978d4fdf3d),
      3.1974301136460107e-17, 3.1974545081948923e-17 },
    { UINT64_C(0xbc826e978d4fdf3d),
      -3.1974545081948929e-17, -3.1974301136460113e-17 },
};
#endif
