#!/usr/bin/env php
<?php

declare(strict_types=1);

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');

require_once $bindingRoot . '/src/autoload.php';

$ffi = null;
$dither = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef struct sixel_dither sixel_dither_t;
sixel_dither_t *sixel_dither_create(int ncolors);
void sixel_dither_destroy(sixel_dither_t *dither);
void sixel_dither_set_palette(sixel_dither_t *dither, unsigned char *palette);
unsigned char *sixel_dither_get_palette(sixel_dither_t *dither);
int sixel_dither_get_num_of_palette_colors(sixel_dither_t *dither);
CDEF, $libPath);

    $dither = $ffi->sixel_dither_create(2);
    if ($dither === null || FFI::isNull($dither)) {
        throw new RuntimeException('sixel_dither_create returned null pointer');
    }

    $expected = $ffi->new('unsigned char[6]');
    $expected[0] = 0;
    $expected[1] = 0;
    $expected[2] = 0;
    $expected[3] = 255;
    $expected[4] = 255;
    $expected[5] = 255;

    $ffi->sixel_dither_set_palette($dither, $expected);

    $palette = $ffi->sixel_dither_get_palette($dither);
    if ($palette === null || FFI::isNull($palette)) {
        throw new RuntimeException('sixel_dither_get_palette returned null pointer');
    }

    $actual = FFI::string($palette, 6);
    $count = (int)$ffi->sixel_dither_get_num_of_palette_colors($dither);

    $ffi->sixel_dither_destroy($dither);
    $dither = null;

    if ($actual === "\x00\x00\x00\xff\xff\xff" && $count >= 2) {
        echo "ok 1 - dither set_palette and palette getter are callable\n";
    } else {
        echo "not ok 1 - dither palette roundtrip returned unexpected values\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - dither palette roundtrip check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_destroy($dither);
        } catch (Throwable $e) {
        }
    }
}
