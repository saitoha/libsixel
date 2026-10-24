/*
 * Print SHA-256 digests of one stream's RGBA, byte-index, and 16-bit-index
 * decoder outputs, for comparing decoder builds on retained streams.
 *
 * usage: verify-decoder STREAM.six
 * output: one line per format:
 *   FORMAT WIDTH HEIGHT NCOLORS PIXEL_SHA256 PALETTE_SHA256|-
 *
 * SIXEL_DECODER_THREADING_GLOBAL_BUDGET supplies the decoder worker budget.
 * SHA-256 is
 * implemented here so the tool has no dependency beyond libsixel.
 */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

/* The wide entry point is internal but exported for regression tests. */
SIXELSTATUS sixel_decode_wide(unsigned char *p, int len,
                              unsigned short **pixels, int *width,
                              int *height, unsigned char **palette,
                              int *ncolors, sixel_allocator_t *allocator);

static uint32_t const k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void
block(uint32_t *h, unsigned char const *p)
{
    uint32_t w[64];
    uint32_t a[8];
    uint32_t s0;
    uint32_t s1;
    uint32_t t1;
    uint32_t t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 |
            (uint32_t)p[i * 4 + 2] << 8 | (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    memcpy(a, h, sizeof(a));
    for (i = 0; i < 64; i++) {
        s1 = ROR(a[4], 6) ^ ROR(a[4], 11) ^ ROR(a[4], 25);
        t1 = a[7] + s1 + ((a[4] & a[5]) ^ (~a[4] & a[6])) + k[i] + w[i];
        s0 = ROR(a[0], 2) ^ ROR(a[0], 13) ^ ROR(a[0], 22);
        t2 = s0 + ((a[0] & a[1]) ^ (a[0] & a[2]) ^ (a[1] & a[2]));
        memmove(a + 1, a, sizeof(uint32_t) * 7);
        a[4] += t1;
        a[0] = t1 + t2;
    }
    for (i = 0; i < 8; i++) {
        h[i] += a[i];
    }
}

static void
sha256_hex(unsigned char const *data, size_t len, char *out)
{
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    unsigned char tail[128];
    uint64_t bits;
    size_t full;
    size_t rest;
    size_t n;
    int i;

    full = len / 64u;
    for (n = 0; n < full; n++) {
        block(h, data + n * 64u);
    }
    rest = len - full * 64u;
    memset(tail, 0, sizeof(tail));
    if (rest > 0) {
        memcpy(tail, data + full * 64u, rest);
    }
    tail[rest] = 0x80;
    n = rest < 56u ? 64u : 128u;
    bits = (uint64_t)len * 8u;
    for (i = 0; i < 8; i++) {
        tail[n - 1 - (size_t)i] = (unsigned char)(bits >> (8 * i));
    }
    block(h, tail);
    if (n == 128u) {
        block(h, tail + 64);
    }
    for (i = 0; i < 8; i++) {
        sprintf(out + i * 8, "%08x", h[i]);
    }
}

static void
emit(char const *name, int w, int h, int ncolors,
     unsigned char const *px, size_t bytes,
     unsigned char const *pal, size_t pal_bytes)
{
    char a[65];
    char b[65];

    sha256_hex(px, bytes, a);
    if (pal != NULL) {
        sha256_hex(pal, pal_bytes, b);
    } else {
        strcpy(b, "-");
    }
    printf("%s %d %d %d %s %s\n", name, w, h, ncolors, a, b);
}

int
main(int argc, char **argv)
{
    FILE *f;
    unsigned char *s;
    unsigned char *px;
    unsigned char *pal;
    unsigned short *wide;
    long len;
    int w;
    int h;
    int nc;
    SIXELSTATUS st;

    if (argc != 2 || (f = fopen(argv[1], "rb")) == NULL ||
            fseek(f, 0, SEEK_END) != 0) {
        return 2;
    }
    len = ftell(f);
    rewind(f);
    if (len <= 0 || len > INT_MAX || (s = malloc((size_t)len)) == NULL ||
            fread(s, 1, (size_t)len, f) != (size_t)len) {
        return 3;
    }
    fclose(f);

    px = NULL;
    st = sixel_decode_direct(s, (int)len, &px, &w, &h, NULL);
    if (SIXEL_FAILED(st)) {
        return 4;
    }
    emit("rgba", w, h, 0, px, (size_t)w * h * 4u, NULL, 0);
    free(px);

    px = NULL;
    pal = NULL;
    st = sixel_decode_raw(s, (int)len, &px, &w, &h, &pal, &nc, NULL);
    if (SIXEL_FAILED(st)) {
        return 5;
    }
    emit("indexed", w, h, nc, px, (size_t)w * h, pal, (size_t)nc * 3u);
    free(px);
    free(pal);

    wide = NULL;
    pal = NULL;
    st = sixel_decode_wide(s, (int)len, &wide, &w, &h, &pal, &nc, NULL);
    if (SIXEL_FAILED(st)) {
        return 6;
    }
    emit("wide", w, h, nc, (unsigned char *)wide,
         (size_t)w * h * sizeof(*wide), pal, (size_t)nc * 3u);
    free(wide);
    free(pal);
    free(s);
    return 0;
}
