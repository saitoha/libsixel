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
void sixel_dither_set_diffusion_type(sixel_dither_t *dither, int method_for_diffuse);
void sixel_dither_set_diffusion_scan(sixel_dither_t *dither, int method_for_scan);
void sixel_dither_set_body_only(sixel_dither_t *dither, int bodyonly);
void sixel_dither_set_optimize_palette(sixel_dither_t *dither, int do_opt);
void sixel_dither_set_pixelformat(sixel_dither_t *dither, int pixelformat);
void sixel_dither_set_transparent(sixel_dither_t *dither, int transparent);
CDEF, $libPath);

    $out = $ffi->new('sixel_dither_t *[1]');
    $status = (int)$ffi->sixel_dither_new($out, 16, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_dither_new failed: ' . $status);
    }

    $dither = $out[0];

    $ffi->sixel_dither_set_diffusion_type($dither, 0x2);
    $ffi->sixel_dither_set_diffusion_scan($dither, 0x2);
    $ffi->sixel_dither_set_body_only($dither, 0);
    $ffi->sixel_dither_set_optimize_palette($dither, 1);
    $ffi->sixel_dither_set_pixelformat($dither, 0x03);
    $ffi->sixel_dither_set_transparent($dither, 0);

    $ffi->sixel_dither_unref($dither);
    $dither = null;

    echo "ok 1 - dither setter APIs accept expected argument values\n";
} catch (Throwable $e) {
    echo "not ok 1 - dither setter API check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_unref($dither);
        } catch (Throwable $e) {
        }
    }
}
