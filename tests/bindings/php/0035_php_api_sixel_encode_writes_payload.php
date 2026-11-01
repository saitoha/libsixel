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

    $payload = '';
    $writeCallback = function ($data, $size, $priv) use (&$payload): int {
        $payload .= FFI::string($data, $size);
        return 0;
    };

    $out = $ffi->new('sixel_output_t *[1]');
    $status = (int)$ffi->sixel_output_new($out, $writeCallback, null, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_output_new failed: ' . $status);
    }
    $output = $out[0];

    $dither = $ffi->sixel_dither_get(0x3);
    if ($dither === null || FFI::isNull($dither)) {
        throw new RuntimeException('sixel_dither_get returned null');
    }

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

    $depth = (int)$ffi->sixel_helper_compute_depth(0x03);
    $status = (int)$ffi->sixel_encode($pixels, 2, 2, $depth, $dither, $output);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_encode failed: ' . $status);
    }

    $ffi->sixel_output_unref($output);
    $output = null;
    $ffi->sixel_dither_unref($dither);
    $dither = null;

    if (strpos($payload, "\x1bP") === 0) {
        echo "ok 1 - sixel_encode writes sixel payload through output callback\n";
    } else {
        echo "not ok 1 - sixel_encode payload missing sixel introducer\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - sixel_encode callback payload check failed\n";
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
