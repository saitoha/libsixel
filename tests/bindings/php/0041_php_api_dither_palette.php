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
unsigned char *sixel_dither_get_palette(sixel_dither_t *dither);
void sixel_dither_set_palette(sixel_dither_t *dither, unsigned char *palette);
CDEF, $libPath);

    $out = $ffi->new('sixel_dither_t *[1]');
    $status = (int)$ffi->sixel_dither_new($out, 2, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_dither_new failed: ' . $status);
    }

    $dither = $out[0];

    $expected = $ffi->new('unsigned char[6]');
    $expected[0] = 1;
    $expected[1] = 2;
    $expected[2] = 3;
    $expected[3] = 4;
    $expected[4] = 5;
    $expected[5] = 6;

    $ffi->sixel_dither_set_palette($dither, $expected);

    $palette = $ffi->sixel_dither_get_palette($dither);
    if ($palette === null || FFI::isNull($palette)) {
        throw new RuntimeException('dither palette getter returned null pointer');
    }

    $actual = FFI::string($palette, 6);

    $ffi->sixel_dither_unref($dither);
    $dither = null;

    if ($actual === "\x01\x02\x03\x04\x05\x06") {
        echo "ok 1 - dither palette getter/setter APIs are callable\n";
    } else {
        echo "not ok 1 - dither palette getter returned unexpected data\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - dither palette API check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_unref($dither);
        } catch (Throwable $e) {
        }
    }
}
