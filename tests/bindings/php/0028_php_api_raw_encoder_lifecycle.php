#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');
$sourceRoot = (string) getenv('TOP_SRCDIR');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';


require_once $bindingRoot . '/src/autoload.php';

$ffi = null;
$encoder = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_encoder sixel_encoder_t;
SIXELSTATUS sixel_encoder_new(sixel_encoder_t **ppencoder, void *allocator);
void sixel_encoder_unref(sixel_encoder_t *encoder);
SIXELSTATUS sixel_encoder_setopt(sixel_encoder_t *encoder, int arg, const char *value);
SIXELSTATUS sixel_encoder_encode(sixel_encoder_t *encoder, const char *filename);
CDEF, $libPath);

    $source = $sourceRoot . '/tests/data/inputs/snake_64.png';

    $out = $ffi->new('sixel_encoder_t *[1]');
    $status = (int)$ffi->sixel_encoder_new($out, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_encoder_new failed: ' . $status);
    }

    $encoder = $out[0];

    $status = (int)$ffi->sixel_encoder_setopt($encoder, ord('o'), $devNull);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_encoder_setopt failed: ' . $status);
    }

    $status = (int)$ffi->sixel_encoder_encode($encoder, $source);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_encoder_encode failed: ' . $status);
    }

    $ffi->sixel_encoder_unref($encoder);
    $encoder = null;

    echo "ok 1 - raw encoder APIs create, configure, and release successfully\n";
} catch (Throwable $e) {
    echo "not ok 1 - raw encoder lifecycle check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $encoder !== null) {
        try {
            $ffi->sixel_encoder_unref($encoder);
        } catch (Throwable $e) {
        }
    }
}
