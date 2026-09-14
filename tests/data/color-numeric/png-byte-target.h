#ifndef COLOR_FIXTURE_PNG_BYTE_TARGET
#define COLOR_FIXTURE_PNG_BYTE_TARGET
/* Independent original PNG -> stored float -> final byte. */
static struct {
    unsigned int source;
    unsigned int linear;
    unsigned int gamma;
    unsigned char byte;
} const color_png_byte_target[2] = {
    {0x3f02f483u, 0x3e6a573fu, 0x3f040404u, 132},
    {0x3f6175e1u, 0x3f2ee032u, 0x3f585858u, 216},
};
#endif
