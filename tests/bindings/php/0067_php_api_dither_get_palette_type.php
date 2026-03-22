#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');

require_once $bindingRoot . '/src/autoload.php';

$ffi = null;
$dither = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_dither sixel_dither_t;
SIXELSTATUS sixel_dither_new(sixel_dither_t **ppdither, int ncolors, void *allocator);
void sixel_dither_unref(sixel_dither_t *dither);
void sixel_dither_set_palette(sixel_dither_t *dither, unsigned char *palette);
unsigned char *sixel_dither_get_palette(sixel_dither_t *dither);
CDEF, $libPath);

    $out = $ffi->new('sixel_dither_t *[1]');
    $status = (int)$ffi->sixel_dither_new($out, 2, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_dither_new failed: ' . $status);
    }

    $dither = $out[0];

    $paletteIn = $ffi->new('unsigned char[6]');
    $paletteIn[0] = 1;
    $paletteIn[1] = 2;
    $paletteIn[2] = 3;
    $paletteIn[3] = 4;
    $paletteIn[4] = 5;
    $paletteIn[5] = 6;

    $ffi->sixel_dither_set_palette($dither, $paletteIn);

    $paletteOut = $ffi->sixel_dither_get_palette($dither);
    if ($paletteOut === null || FFI::isNull($paletteOut)) {
        throw new RuntimeException('sixel_dither_get_palette returned null pointer');
    }

    $payload = FFI::string($paletteOut, 6);

    $ffi->sixel_dither_unref($dither);
    $dither = null;

    if (is_string($payload) && strlen($payload) >= 6) {
        echo "ok 1 - dither get_palette returns byte payload in current PHP path\n";
    } else {
        echo "not ok 1 - dither get_palette did not return expected byte payload\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - dither get_palette return-type check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_unref($dither);
        } catch (Throwable $e) {
        }
    }
}
