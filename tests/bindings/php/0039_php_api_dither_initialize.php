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
SIXELSTATUS sixel_dither_initialize(sixel_dither_t *dither, unsigned char *data, int width, int height, int pixelformat, int method_for_largest, int method_for_rep, int quality_mode);
int sixel_dither_get_num_of_histogram_colors(sixel_dither_t *dither);
CDEF, $libPath);

    $out = $ffi->new('sixel_dither_t *[1]');
    $status = (int)$ffi->sixel_dither_new($out, 16, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_dither_new failed: ' . $status);
    }

    $dither = $out[0];

    $pixels = $ffi->new('unsigned char[12]');
    $pixels[0] = 255;
    $pixels[1] = 0;
    $pixels[2] = 0;
    $pixels[3] = 0;
    $pixels[4] = 255;
    $pixels[5] = 0;
    $pixels[6] = 0;
    $pixels[7] = 0;
    $pixels[8] = 255;
    $pixels[9] = 255;
    $pixels[10] = 255;
    $pixels[11] = 255;

    $status = (int)$ffi->sixel_dither_initialize($dither, $pixels, 2, 2, 0x03, 0, 0, 0);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_dither_initialize failed: ' . $status);
    }

    $histogram = (int)$ffi->sixel_dither_get_num_of_histogram_colors($dither);
    $ffi->sixel_dither_unref($dither);
    $dither = null;

    if ($histogram > 0) {
        echo "ok 1 - dither initialize updated histogram state ({$histogram})\n";
    } else {
        echo "not ok 1 - dither initialize did not update histogram state\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - dither initialize check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_unref($dither);
        } catch (Throwable $e) {
        }
    }
}
