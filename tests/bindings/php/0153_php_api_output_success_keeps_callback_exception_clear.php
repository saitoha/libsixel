#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');

require_once $bindingRoot . '/src/autoload.php';

$ffi = null;
$output = null;
$dither = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_output sixel_output_t;
typedef struct sixel_dither sixel_dither_t;
typedef int (*sixel_write_function)(char *data, int size, void *priv);
SIXELSTATUS sixel_output_new(sixel_output_t **output, sixel_write_function fn_write, void *priv, void *allocator);
void sixel_output_unref(sixel_output_t *output);
sixel_dither_t *sixel_dither_get(int builtin_dither);
void sixel_dither_unref(sixel_dither_t *dither);
int sixel_helper_compute_depth(int pixelformat);
SIXELSTATUS sixel_encode(unsigned char *pixels, int width, int height, int depth, sixel_dither_t *dither, sixel_output_t *context);
CDEF, $libPath);

    $calls = 0;

    $out = $ffi->new('sixel_output_t *[1]');
    $status = (int)$ffi->sixel_output_new($out, function ($chunk, $size, $priv) use (&$calls): int {
        $calls += 1;
        return 0;
    }, null, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_output_new failed: ' . $status);
    }
    $output = $out[0];

    $dither = $ffi->sixel_dither_get(0x3);
    if ($dither === null || FFI::isNull($dither)) {
        throw new RuntimeException('sixel_dither_get returned null pointer');
    }

    $depth = (int)$ffi->sixel_helper_compute_depth(0x03);
    $pixels = $ffi->new('unsigned char[3]');
    $pixels[0] = 255;
    $pixels[1] = 0;
    $pixels[2] = 0;

    $first = (int)$ffi->sixel_encode($pixels, 1, 1, $depth, $dither, $output);
    $second = (int)$ffi->sixel_encode($pixels, 1, 1, $depth, $dither, $output);

    $ffi->sixel_output_unref($output);
    $output = null;
    $ffi->sixel_dither_unref($dither);
    $dither = null;

    if (!API::failed($first) && !API::failed($second) && $calls >= 2) {
        echo "ok 1 - successful output callback remains clear across repeated encode\n";
    } else {
        echo "not ok 1 - successful output callback behavior diverged across repeated encode\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - successful output callback repeated-encode check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $output !== null) {
        try {
            $ffi->sixel_output_unref($output);
        } catch (Throwable $e) {
        }
    }
    if ($ffi !== null && $dither !== null) {
        try {
            $ffi->sixel_dither_unref($dither);
        } catch (Throwable $e) {
        }
    }
}
