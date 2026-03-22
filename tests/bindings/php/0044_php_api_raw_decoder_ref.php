#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');


require_once $bindingRoot . '/src/autoload.php';

$ffi = null;
$decoder = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_decoder sixel_decoder_t;
SIXELSTATUS sixel_decoder_new(sixel_decoder_t **ppdecoder, void *allocator);
void sixel_decoder_ref(sixel_decoder_t *decoder);
void sixel_decoder_unref(sixel_decoder_t *decoder);
CDEF, $libPath);

    $out = $ffi->new('sixel_decoder_t *[1]');
    $status = (int)$ffi->sixel_decoder_new($out, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_decoder_new failed: ' . $status);
    }

    $decoder = $out[0];
    $ffi->sixel_decoder_ref($decoder);
    $ffi->sixel_decoder_unref($decoder);
    $ffi->sixel_decoder_unref($decoder);
    $decoder = null;

    echo "ok 1 - raw decoder ref/unref APIs are callable\n";
} catch (Throwable $e) {
    echo "not ok 1 - raw decoder ref/unref check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $decoder !== null) {
        try {
            $ffi->sixel_decoder_unref($decoder);
        } catch (Throwable $e) {
        }
    }
}
