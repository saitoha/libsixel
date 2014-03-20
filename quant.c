
/*****************************************************************************
 *
 * Pixel object
 *
 *****************************************************************************/

typedef struct _stbex_pixel {
    union {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };
        uint32_t color_index;
    };
} stbex_pixel;

stbex_pixel
stbex_pixel_new(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    stbex_pixel p;

    p.r = r;
    p.g = g;
    p.b = b; 
    p.a = a;

    return p;
}

int
stbex_pixel_compare_r(const stbex_pixel *lhs, const stbex_pixel *rhs)
{
    return lhs->b > rhs->b ? 1: -1;
}

int
stbex_pixel_compare_g(const stbex_pixel *lhs, const stbex_pixel *rhs)
{
    return lhs->g > rhs->g ? 1: -1;
}

int
stbex_pixel_compare_b(const stbex_pixel *lhs, const stbex_pixel *rhs)
{
    return lhs->b > rhs->b ? 1: -1;
}

void
stbex_pixel_sort_r(stbex_pixel * const pixels, size_t npixels)
{
    qsort(pixels, npixels, sizeof(stbex_pixel),
          (int (*)(const void *, const void *))stbex_pixel_compare_r);
}

void
stbex_pixel_sort_g(stbex_pixel * const pixels, size_t npixels)
{
    qsort(pixels, npixels, sizeof(stbex_pixel),
          (int (*)(const void *, const void *))stbex_pixel_compare_g);
}

void
stbex_pixel_sort_b(stbex_pixel * const pixels, size_t npixels)
{
    qsort(pixels, npixels, sizeof(stbex_pixel),
          (int (*)(const void *, const void *))stbex_pixel_compare_b);
}


/*****************************************************************************
 *
 * Median cut
 *
 *****************************************************************************/

/** cube */
struct stbex_cube;
typedef struct _stbex_cube {
    uint8_t min_r;
    uint8_t min_g;
    uint8_t min_b;
    uint8_t max_r;
    uint8_t max_g;
    uint8_t max_b;
    size_t npixels;
    stbex_pixel *pixels;
    struct stbex_cube *left;
    struct stbex_cube *right;
    struct stbex_cube *parent;
} stbex_cube;

void
stbex_cube_fit(stbex_cube *cube)
{
    int i;
    stbex_pixel *p;

    cube->max_r = 0;
    cube->min_r = 255;
    cube->max_g = 0;
    cube->min_g = 255;
    cube->max_b = 0;
    cube->min_b = 255;

    for (i = 0; i < cube->npixels; i++) {
        p = cube->pixels + i;
        if (p->r < cube->min_r) {
            cube->min_r = p->r;
        }
        if (p->g < cube->min_g) {
            cube->min_g = p->g;
        }
        if (p->b < cube->min_b) {
            cube->min_b = p->b;
        }
        if (p->r > cube->max_r) {
            cube->max_r = p->r;
        }
        if (p->g > cube->max_g) {
            cube->max_g = p->g;
        }
        if (p->b > cube->max_b) {
            cube->max_b = p->b;
        }
    }
}

struct stbex_cube *
stbex_cube_new(stbex_pixel *pixels, size_t npixels, stbex_cube *parent)
{
    stbex_cube *cube;
   
    cube = malloc(sizeof(stbex_cube));
    cube->pixels = malloc(sizeof(stbex_pixel *) * npixels);
    memcpy(cube->pixels, pixels, sizeof(stbex_pixel *) * npixels);
    cube->npixels = npixels;
    cube->left = NULL;
    cube->right = NULL;
    cube->parent = (struct stbex_cube*)parent;

    stbex_cube_fit(cube);

    return (struct stbex_cube *)cube;
}

void
stbex_cube_free(stbex_cube *cube, stbex_pixel *pixels)
{
    free(cube->pixels);
    free(cube);
}

int
stbex_cube_hatch(stbex_cube *cube, int threshold)
{
    int length_r;
    int length_g;
    int length_b;
    int divide_point;
    int divide_value = 0;

    if (cube->left != NULL && cube->right != NULL) {
        return stbex_cube_hatch((stbex_cube *)cube->left, threshold)
             + stbex_cube_hatch((stbex_cube *)cube->right, threshold);
    }

    length_r = (int)cube->max_r - (int)cube->min_r;
    length_g = (int)cube->max_g - (int)cube->min_g;
    length_b = (int)cube->max_b - (int)cube->min_b;

    if (cube->npixels <= 8) {
        return cube->npixels;
    }
           
    if (cube->npixels < threshold) {
        if (length_r < 16 && length_g < 16 && length_b < 16) {
            return 1;
        }
        return 0;
    }

    divide_point = cube->npixels / 2;

    if (length_r > length_g && length_r > length_b) {
        stbex_pixel_sort_r(cube->pixels, cube->npixels);
        divide_value = cube->pixels[divide_point - 1].r;
        for (; divide_point < cube->npixels; divide_point++) {
            if (cube->pixels[divide_point].r != divide_value) {
                break;
            }
        }
    } else if (length_g > length_b) {
        stbex_pixel_sort_g(cube->pixels, cube->npixels);
        divide_value = cube->pixels[divide_point - 1].g;
        for (; divide_point < cube->npixels; divide_point++) {
            if (cube->pixels[divide_point].g != divide_value) {
                break;
            }
        }
    } else {
        stbex_pixel_sort_b(cube->pixels, cube->npixels);
        divide_value = cube->pixels[divide_point - 1].b;
        for (; divide_point < cube->npixels; divide_point++) {
            if (cube->pixels[divide_point].b != divide_value) {
                break;
            }
        }
    }

    if (divide_point == cube->npixels) {
        return 1;
    }

    if (cube->npixels == divide_point + 1) {
        return 1;
    }

    cube->left = stbex_cube_new(cube->pixels, divide_point, cube);
    cube->right = stbex_cube_new(cube->pixels + divide_point + 1,
                           cube->npixels - divide_point - 1, cube);
    cube->npixels = 0;

    return 2;
}

void
stbex_cube_get_sample(stbex_cube *cube, stbex_pixel *samples, stbex_pixel *results, int *nresults)
{
    int length_r;
    int length_g;
    int length_b;

    if (cube->left) {
        stbex_cube_get_sample((stbex_cube *)cube->left, samples, results, nresults);
        stbex_cube_get_sample((stbex_cube *)cube->right, samples, results, nresults);
    } else {

        length_r = (int)cube->max_r - (int)cube->min_r;
        length_g = (int)cube->max_g - (int)cube->min_g;
        length_b = (int)cube->max_b - (int)cube->min_b;

        if (length_r < 16 && length_g < 16 && length_b < 16) {
            *(results + (*nresults)++) = stbex_pixel_new((cube->min_r + cube->max_r) / 2, (cube->min_g + cube->max_g) / 2, (cube->min_b + cube->max_b) / 2, 0); 
/*
            printf("(%d, %d, %d)\n", (cube->min_r + cube->max_r) / 2, (cube->min_g + cube->max_g) / 2, (cube->min_b + cube->max_b) / 2);
*/
        } else {
            *(results + (*nresults)++) = stbex_pixel_new(cube->min_r, cube->min_g, cube->min_b, 0); 
            *(results + (*nresults)++) = stbex_pixel_new(cube->max_r, cube->min_g, cube->min_b, 0); 
            *(results + (*nresults)++) = stbex_pixel_new(cube->min_r, cube->max_g, cube->min_b, 0); 
            *(results + (*nresults)++) = stbex_pixel_new(cube->min_r, cube->min_g, cube->max_b, 0); 
            *(results + (*nresults)++) = stbex_pixel_new(cube->max_r, cube->max_g, cube->min_b, 0); 
            *(results + (*nresults)++) = stbex_pixel_new(cube->min_r, cube->max_g, cube->max_b, 0); 
            *(results + (*nresults)++) = stbex_pixel_new(cube->max_r, cube->min_g, cube->max_b, 0); 
            *(results + (*nresults)++) = stbex_pixel_new(cube->max_r, cube->max_g, cube->max_b, 0); 
/*
            printf("(%d, %d, %d) - (%d, %d, %d) => %ld\n",
                            cube->min_r,
                            cube->min_g,
                            cube->min_b,
                            cube->max_r,
                            cube->max_g,
                            cube->max_b,
                            cube->npixels);
*/
        }
    }
}

/*****************************************************************************
 *
 * Coulor reduction
 *
 *****************************************************************************/

void
pset(uint8_t *data, int index, int depth, stbex_pixel *value)
{
    memcpy(data + index * depth, value, depth);
}

stbex_pixel *
pget(unsigned char *data, int index, int depth)
{
    return (stbex_pixel *)(data + index * depth);
}

stbex_pixel *
zigzag_pget(unsigned char *data, int index, int width, int depth)
{
    int n = (int)floor(sqrt((index + 1) * 8) * 0.5 - 0.5);
    int x, y;

    if ((n & 0x1) == 0) {
        y = index - n * (n + 1) / 2;
        x = n - y;
    } else {
        x = index - n * (n + 1) / 2;
        y = n - x;
    }
    return (stbex_pixel *)(data + (y * width + x) * depth);
}

stbex_pixel *
get_sample(unsigned char *data, int width, int height, int depth, int *count)
{
    int i, j;
    int n = width * height / *count;
    int index;
    stbex_pixel *result = malloc(sizeof(stbex_pixel) * *count);
    stbex_pixel p;
    char histgram[1 << 15];

    memset(histgram, 0, sizeof(histgram));

    for (i = 0; i < *count; i++) {
        /* p = *zigzag_pget(data, i * n, width, depth); */
        p = *pget(data, i * n, depth);
        index = (p.r >> 3) << 10 | (p.g >> 3) << 5 | p.b >> 3;
        histgram[index] = 1;
    }

    for (i = 0, j = 0; i < sizeof(histgram); i++) {
        if (histgram[i] != 0) {
            result[j].r = (i >> 10 & 0x1f) << 3;
            result[j].g = (i >> 5 & 0x1f) << 3;
            result[j].b = (i & 0x1f) << 3;
            j++;
        }
    }
    *count = j;
    return result;
}

unsigned char *
make_palette(unsigned char *data, int x, int y, int n, int c)
{
    int i;
    unsigned char *palette;
    int sample_count = 256;
    stbex_pixel *sample;
    stbex_cube *cube;
    int nresult = 0;
    int ncount;
    int count = 0;

    sample = get_sample(data, x, y, n, &sample_count);
    cube = (stbex_cube *)stbex_cube_new(sample, sample_count, NULL);

    for (ncount = sample_count / 2; ncount > 8; ncount /= 2) {
        count += stbex_cube_hatch(cube, ncount);
    }

    stbex_pixel results[sample_count];
    stbex_cube_get_sample(cube, sample, (stbex_pixel *)results, &nresult);
    free(sample);

/*
    printf("[%d -> %d]\n", sample_count, count); fflush(0);
*/

    palette = malloc(c * n);
    for (i = 0; i < c; i++) {
        memcpy(palette + i * 3, results + i, 3); 
    }
    return palette;
}

void add_offset(unsigned char *data, int i, int n, int roffset, int goffset, int boffset) {
    int r = data[i * n + 0] + roffset;
    int g = data[i * n + 1] + goffset;
    int b = data[i * n + 2] + boffset;

    if (r < 0) {
        r = 0;
    }
    if (g < 0) {
        g = 0;
    }
    if (b < 0) {
        b = 0;
    }
    if (r > 255) {
        r = 255;
    }
    if (g > 255) {
        g = 255;
    }
    if (b > 255) {
        b = 255;
    }

    data[i * n + 0] = (unsigned char)r;
    data[i * n + 1] = (unsigned char)g;
    data[i * n + 2] = (unsigned char)b;
}


unsigned char *
apply_palette(unsigned char *data,
              int width, int height, int depth,
              unsigned char *palette, int c,
              int use_diffusion)
{
    int i;
    int j;
    int x, y;
    int r = 0, g = 0, b = 0;
    int rdiff, gdiff, bdiff;
    int roffset, goffset, boffset;
    int distant;
    int diff;
    int index;
    unsigned char *result;

    result = malloc(width * height);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            i = y * width + x;
            r = data[i * depth + 0];
            g = data[i * depth + 1];
            b = data[i * depth + 2];
            diff = 256 * 256 * 3;
            index = -1;
            j = 1;
            while (1) {
                rdiff = r - (int)palette[j * 3 + 0];
                gdiff = g - (int)palette[j * 3 + 1];
                bdiff = b - (int)palette[j * 3 + 2];
                distant = rdiff * rdiff + gdiff * gdiff + bdiff * bdiff;
                if (distant < diff) {
                    diff = distant;
                    index = j;
                }
                j++;
                if (j == c) {
                    break;
                }
            }
            if (index > 0) {
                result[i] = index;
                if (1) {
                    roffset = (int)data[i * depth + 0] - (int)palette[index * 3 + 0];
                    goffset = (int)data[i * depth + 1] - (int)palette[index * 3 + 1];
                    boffset = (int)data[i * depth + 2] - (int)palette[index * 3 + 2];
                    if (y < height - 1) {
                        add_offset(data, i + width, depth,
                                   roffset * 5 / 16,
                                   goffset * 5 / 16,
                                   boffset * 5 / 16);
                        if (x > 1) {
                            add_offset(data, i + width - 1, depth,
                                       roffset * 3 / 16,
                                       goffset * 3 / 16,
                                       boffset * 3 / 16);
                            roffset -= roffset * 3 / 16;
                            goffset -= goffset * 3 / 16;
                            boffset -= boffset * 3 / 16;
                        }
                        if (x < width - 1) {
                            add_offset(data, i + width + 1, depth,
                                       roffset * 1 / 16,
                                       goffset * 1 / 16,
                                       boffset * 1 / 16);
                        }
                    }
                    if (x < width - 1) {
                        roffset -= roffset * 5 / 16;
                        goffset -= goffset * 5 / 16;
                        boffset -= boffset * 5 / 16;
                        roffset -= roffset * 3 / 16;
                        goffset -= goffset * 3 / 16;
                        boffset -= boffset * 3 / 16;
                        roffset -= roffset * 1 / 16;
                        goffset -= goffset * 1 / 16;
                        boffset -= boffset * 1 / 16;
                        add_offset(data, i + 1, depth,
                                   roffset,
                                   goffset,
                                   boffset);
                    }
                }
            }
        }
    }
    return result;
}

// EOF
