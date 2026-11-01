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
CDEF, $libPath);

    $out = $ffi->new('sixel_dither_t *[1]');
    $status = (int)$ffi->sixel_dither_new($out, 2, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_dither_new failed: ' . $status);
    }

    $dither = $out[0];

    $palette = $ffi->new('unsigned char[6]');
    $palette[0] = 0;
    $palette[1] = 0;
    $palette[2] = 0;
    $palette[3] = 256;
    $palette[4] = 0;
    $palette[5] = 0;

    $ffi->sixel_dither_set_palette($dither, $palette);

    $ffi->sixel_dither_unref($dither);
    $dither = null;

    echo "ok 1 - dither set_palette accepts out-of-range component in current PHP FFI path\n";
} catch (Throwable $e) {
    echo "not ok 1 - dither set_palette out-of-range acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_unref($dither);
        } catch (Throwable $e) {
        }
    }
}
