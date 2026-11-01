/* SPDX-License-Identifier: MIT */
/* Verify all four APNG output frames against independent colors. */
int test_animation_loader(char const *, char const *, char const *,
                           char const *, int);

int
test_anim_native_0018(int argc, char **argv)
{
    return argc == 3 ? test_animation_loader(argv[1], argv[2],
        "libpng:cms_engine=none!", "1", 0) : 1;
}
