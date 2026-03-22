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
typedef struct sixel_dither sixel_dither_t;
sixel_dither_t *sixel_dither_create(int ncolors);
void sixel_dither_destroy(sixel_dither_t *dither);
CDEF, $libPath);

    $dither = $ffi->sixel_dither_create(16);
    if ($dither === null || FFI::isNull($dither)) {
        throw new RuntimeException('sixel_dither_create returned null pointer');
    }

    $ffi->sixel_dither_destroy($dither);
    $dither = null;

    echo "ok 1 - dither destroy API is callable\n";
} catch (Throwable $e) {
    echo "not ok 1 - dither destroy API check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_destroy($dither);
        } catch (Throwable $e) {
        }
    }
}
