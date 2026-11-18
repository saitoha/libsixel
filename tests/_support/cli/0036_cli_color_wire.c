/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/data/color-numeric/cli-color-wire.h"

/* This deliberately small reader observes the wire independently of the
 * production SIXEL decoder. It accepts the RGB/raster/RLE syntax emitted by
 * these tests, with bounded storage for the largest 8x12 witness. */
static int
cli_wire_error(char const *message)
{
    fprintf(stderr, "CLI color wire: %s\n", message);
    return 1;
}

static int
cli_wire_count(char const *text, char const *needle)
{
    char const *p;
    int count;

    count = 0;
    p = text;
    while ((p = strstr(p, needle)) != NULL) {
        ++count;
        p += strlen(needle);
    }
    return count;
}

static int
cli_wire_record(char const *text, char const *record)
{
    char const *p;

    p = strstr(text, record);
    if (p == NULL || cli_wire_count(text, record) != 1) {
        return 0;
    }
    p += strlen(record);
    return *p == '\0' || *p == '\n' || *p == '\r';
}

static int
cli_wire_params(char const **position, char const *end, int values[6])
{
    char const *p;
    int count;
    int value;

    p = *position;
    count = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (count == 6) {
            return -1;
        }
        value = 0;
        do {
            value = value * 10 + (*p++ - '0');
            if (value > 65535) {
                return -1;
            }
        } while (p < end && *p >= '0' && *p <= '9');
        values[count++] = value;
        if (p == end || *p != ';') {
            break;
        }
        ++p;
        if (p == end || *p < '0' || *p > '9') {
            return -1;
        }
    }
    *position = p;
    return count;
}

static int
cli_wire_bpc_registers(int bpc, int count, unsigned char const defined[256],
                       unsigned char palette[256][3])
{
    /* For each input word w and coverage a, set x=(a*w+(1-a)*4096)/65535
     * with a normalized by 65535. The input ICC produces (1+3*x)/4;
     * enabling BPC changes it to (1+x)/2. The two seed-1 cluster means
     * average samples 0..3 and 4..5 in sRGB transfer coordinates. Undo
     * that transfer on each mean, then apply the destination ICC inverse
     * max(0, 2*Y-1). In register order the resulting percentages are
     * 66.60958 and 0 (off), or 12.63410 and 78.17860 (on). The nearest
     * half-step margin exceeds 0.109, versus 0.009 from propagating the
     * fixture's 3e-5 linear sample budget through the monotone transfers
     * and the destination scale over these two cluster ranges. */
    static unsigned char const expected[2][2][3] = {
        {{67, 67, 67}, {0, 0, 0}},
        {{13, 13, 13}, {78, 78, 78}},
    };

    if (count != 2 || !defined[0] || !defined[1] ||
        memcmp(palette, expected[bpc], sizeof(expected[bpc])) != 0) {
        return cli_wire_error("output BPC RGB101 register definitions");
    }
    return 0;
}

static int
cli_wire_bpc_indices(int bpc, unsigned char const painted[96],
                     unsigned char const indices[96])
{
    /* Seed 1 puts the darker four samples in register 1 with BPC off and
     * register 0 with BPC on. The squared-distance gap between both gamma
     * cluster means is greater than 0.013 in each scalar gray coordinate
     * for every sample. Check the actual paint-time register IDs. */
    static unsigned char const expected[2][6] = {
        {1, 1, 1, 1, 0, 0},
        {0, 0, 0, 0, 1, 1},
    };
    int x;

    for (x = 0; x < 6; ++x) {
        if (!painted[x] || indices[x] != expected[bpc][x]) {
            return cli_wire_error("output BPC paint-time register index");
        }
    }
    return 0;
}

int
test_cli_0036_color_wire(int argc, char **argv)
{
    unsigned char palette[256][3];
    unsigned char defined[256];
    unsigned char pixels[96][3];
    unsigned char painted[96];
    unsigned char indices[96];
    char loader_record[96];
    char const *trace;
    char const *loader;
    char const *p;
    char const *end;
    int values[6];
    int specimen;
    int count;
    int width;
    int height;
    int raster;
    int color;
    int x;
    int y;
    int repeat;
    int bits;
    int bit;
    int i;
    int c;
    int source;
    int cms_expected;
    int bpc;
    int check_registers;
    int register_count;

    if (argc != 3 && argc != 4) {
        return cli_wire_error("expected loader name and captured CLI output");
    }
    bpc = -1;
    check_registers = 0;
    if (argc == 4) {
        if (strcmp(argv[3], "bpc-off-registers") == 0 ||
            strcmp(argv[3], "bpc-off-indices") == 0) {
            bpc = 0;
        } else if (strcmp(argv[3], "bpc-on-registers") == 0 ||
                   strcmp(argv[3], "bpc-on-indices") == 0) {
            bpc = 1;
        } else {
            return cli_wire_error("unknown wire observation");
        }
        check_registers = strstr(argv[3], "-registers") != NULL;
    }
    specimen = strcmp(argv[1], "libtiff-ycbcr") == 0 ? 2
             : strcmp(argv[1], "libtiff") == 0 ? 1 : 0;
    loader = specimen == 2 ? "libtiff" : argv[1];
    if (strcmp(argv[1], "libpng-p3-8") == 0) {
        specimen = 3;
        loader = "libpng";
    } else if (strcmp(argv[1], "libpng-p3-16") == 0) {
        specimen = 4;
        loader = "libpng";
    } else if (strcmp(argv[1], "libpng-srgb-p3") == 0) {
        specimen = 5;
        loader = "libpng";
    } else if (strcmp(argv[1], "libtiff-p3") == 0) {
        specimen = 6;
        loader = "libtiff";
    }
    if (strcmp(argv[1], "libpng-p3-16-fs2") == 0) {
        specimen = 7;
        loader = "libpng";
    }
    if (strcmp(argv[1], "libpng-srgb-16") == 0) {
        specimen = 8;
        loader = "libpng";
    }
    cms_expected = specimen == 5 ? 0 : 1;
    if (specimen == 0 && strcmp(argv[1], "libpng") != 0 &&
        strcmp(argv[1], "builtin") != 0) {
        return cli_wire_error("unknown loader");
    }
    trace = argv[2];
    if (specimen == 7 && (cli_wire_count(trace, "LSXDTH1|") != 1 ||
            strstr(trace, "LSXDTH1|rc=0|diffuse=fs|scan=raster|") == NULL)) {
        return cli_wire_error("actual Floyd-Steinberg dispatch missing");
    }
    snprintf(loader_record, sizeof(loader_record),
             "LSXLOAD1|event=try|loader=%s|code=L_TRY", loader);
    if (!cli_wire_record(trace, loader_record)) {
        return cli_wire_error("selected loader was not attempted");
    }
    snprintf(loader_record, sizeof(loader_record),
             "LSXLOAD1|event=ok|loader=%s|code=L_OK", loader);
    if (cli_wire_count(trace, "LSXLOAD1|event=try|") != 1 ||
        cli_wire_count(trace, "LSXLOAD1|event=ok|") != 1 ||
        !cli_wire_record(trace, loader_record)) {
        return cli_wire_error("wrong actual loader");
    }
    if (bpc < 0 &&
        (cli_wire_count(trace, "LSXCMS2|event=apply|") != cms_expected ||
         (cms_expected != 0 && !cli_wire_record(trace,
            "LSXCMS2|event=apply|engine=builtin|"
            "path=extended-rgb|src=4|dst=4|ok=1")))) {
        return cli_wire_error("wrong actual CMS application");
    }
    p = strstr(trace, "\033P");
    if (p == NULL || cli_wire_count(trace, "\033P") != 1) {
        return cli_wire_error("expected one DCS frame");
    }
    p += 2;
    end = strstr(p, "\033\\");
    if (end == NULL) {
        return cli_wire_error("missing ST");
    }
    memset(values, 0, sizeof(values));
    count = cli_wire_params(&p, end, values);
    /* Missing DCS parameters have their standard zero defaults. Opaque
     * output omits P2; the PNG mask requires explicit keep semantics. */
    if (count < 0 || count > 3 || p == end || *p++ != 'q' ||
        values[1] != (bpc >= 0 ? 0 : color_cli_wire[specimen].p2)) {
        return cli_wire_error("DCS parameters or P2");
    }
    width = bpc >= 0 ? 6 : color_cli_wire[specimen].width;
    height = bpc >= 0 ? 1 : color_cli_wire[specimen].height;
    memset(defined, 0, sizeof(defined));
    memset(painted, 0, sizeof(painted));
    memset(pixels, 0, sizeof(pixels));
    raster = 0;
    register_count = 0;
    color = -1;
    x = 0;
    y = 0;
    while (p < end) {
        if (*p == '"') {
            ++p;
            count = cli_wire_params(&p, end, values);
            if (raster++ || count != 4 || values[0] != 1 ||
                values[1] != 1 || values[2] != width ||
                values[3] != height) {
                return cli_wire_error("raster dimensions");
            }
            continue;
        }
        if (*p == '#') {
            ++p;
            count = cli_wire_params(&p, end, values);
            if ((count != 1 && count != 5) || values[0] > 255) {
                return cli_wire_error("palette parameters");
            }
            color = values[0];
            if (count == 5) {
                ++register_count;
                if (values[1] != 2) {
                    return cli_wire_error("expected RGB palette");
                }
                for (c = 0; c < 3; ++c) {
                    if (values[c + 2] > 100) {
                        return cli_wire_error("palette outside RGB101");
                    }
                    palette[color][c] = (unsigned char)values[c + 2];
                }
                defined[color] = 1;
            }
            continue;
        }
        if (*p == '$' || *p == '-') {
            x = 0;
            if (*p++ == '-') {
                y += 6;
            }
            continue;
        }
        repeat = 1;
        if (*p == '!') {
            ++p;
            count = cli_wire_params(&p, end, values);
            if (count != 1 || values[0] == 0) {
                return cli_wire_error("invalid repeat");
            }
            repeat = values[0];
        }
        if (!raster || p == end || *p < '?' || *p > '~' ||
            repeat > width - x) {
            return cli_wire_error("invalid drawing command or extent");
        }
        bits = *p++ - '?';
        for (i = 0; i < repeat; ++i, ++x) {
            for (bit = 0; bit < 6; ++bit) {
                if ((bits & (1 << bit)) == 0) {
                    continue;
                }
                if (y + bit >= height || color < 0 || !defined[color]) {
                    return cli_wire_error("paint outside raster or palette");
                }
                /* Resolve the palette at paint time. Later redefinitions
                 * must not retroactively recolor previously drawn pixels. */
                memcpy(pixels[(y + bit) * width + x], palette[color], 3u);
                painted[(y + bit) * width + x] = 1;
                indices[(y + bit) * width + x] = (unsigned char)color;
            }
        }
    }
    if (!raster) {
        return cli_wire_error("missing raster attributes");
    }
    if (bpc >= 0) {
        if (check_registers) {
            return cli_wire_bpc_registers(bpc, register_count, defined,
                                          palette);
        }
        return cli_wire_bpc_indices(bpc, painted, indices);
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            source = (y / color_cli_wire[specimen].scale) *
                     color_cli_wire[specimen].source_width +
                     x / color_cli_wire[specimen].scale;
            if (source >= color_cli_wire[specimen].count ||
                painted[y * width + x] ==
                    color_cli_wire[specimen].hidden[source]) {
                return cli_wire_error("coverage or source position");
            }
            if (!painted[y * width + x]) {
                continue;
            }
            if (memcmp(pixels[y * width + x],
                       color_cli_wire[specimen].rgb[source], 3u) != 0) {
                fprintf(stderr, "CLI color wire: pixel %d,%d "
                        "actual=%u,%u,%u expected=%u,%u,%u\n", x, y,
                        pixels[y * width + x][0],
                        pixels[y * width + x][1],
                        pixels[y * width + x][2],
                        color_cli_wire[specimen].rgb[source][0],
                        color_cli_wire[specimen].rgb[source][1],
                        color_cli_wire[specimen].rgb[source][2]);
                return 1;
            }
        }
    }
    return 0;
}
