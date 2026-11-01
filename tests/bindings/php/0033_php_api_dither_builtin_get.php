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
sixel_dither_t *sixel_dither_get(int builtin_dither);
int sixel_dither_get_num_of_palette_colors(sixel_dither_t *dither);
void sixel_dither_unref(sixel_dither_t *dither);
CDEF, $libPath);

    $dither = $ffi->sixel_dither_get(0x3);
    if ($dither === null || FFI::isNull($dither)) {
        throw new RuntimeException('sixel_dither_get returned null');
    }

    $paletteColors = (int)$ffi->sixel_dither_get_num_of_palette_colors($dither);
    $ffi->sixel_dither_unref($dither);
    $dither = null;

    if ($paletteColors > 0) {
        echo "ok 1 - built-in dither returned usable context ({$paletteColors} colors)\n";
    } else {
        echo "not ok 1 - built-in dither returned no palette colors\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - dither builtin get check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_unref($dither);
        } catch (Throwable $e) {
        }
    }
}
