#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');


require_once $bindingRoot . '/src/autoload.php';

$ffi = null;
$encoder = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_encoder sixel_encoder_t;
SIXELSTATUS sixel_encoder_new(sixel_encoder_t **ppencoder, void *allocator);
void sixel_encoder_ref(sixel_encoder_t *encoder);
void sixel_encoder_unref(sixel_encoder_t *encoder);
CDEF, $libPath);

    $out = $ffi->new('sixel_encoder_t *[1]');
    $status = (int)$ffi->sixel_encoder_new($out, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_encoder_new failed: ' . $status);
    }

    $encoder = $out[0];
    $ffi->sixel_encoder_ref($encoder);
    $ffi->sixel_encoder_unref($encoder);
    $ffi->sixel_encoder_unref($encoder);
    $encoder = null;

    echo "ok 1 - raw encoder ref/unref APIs are callable\n";
} catch (Throwable $e) {
    echo "not ok 1 - raw encoder ref/unref check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $encoder !== null) {
        try {
            $ffi->sixel_encoder_unref($encoder);
        } catch (Throwable $e) {
        }
    }
}
