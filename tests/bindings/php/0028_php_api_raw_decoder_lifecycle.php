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
$decoder = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_decoder sixel_decoder_t;
SIXELSTATUS sixel_decoder_new(sixel_decoder_t **ppdecoder, void *allocator);
void sixel_decoder_unref(sixel_decoder_t *decoder);
SIXELSTATUS sixel_decoder_setopt(sixel_decoder_t *decoder, int arg, const char *value);
SIXELSTATUS sixel_decoder_decode(sixel_decoder_t *decoder);
CDEF, $libPath);

    $source = $sourceRoot . '/tests/data/inputs/snake_64.six';

    $out = $ffi->new('sixel_decoder_t *[1]');
    $status = (int)$ffi->sixel_decoder_new($out, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_decoder_new failed: ' . $status);
    }

    $decoder = $out[0];

    $status = (int)$ffi->sixel_decoder_setopt($decoder, ord('i'), $source);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_decoder_setopt(input) failed: ' . $status);
    }

    $status = (int)$ffi->sixel_decoder_setopt($decoder, ord('o'), $devNull);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_decoder_setopt(output) failed: ' . $status);
    }

    $status = (int)$ffi->sixel_decoder_decode($decoder);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_decoder_decode failed: ' . $status);
    }

    $ffi->sixel_decoder_unref($decoder);
    $decoder = null;

    echo "ok 1 - raw decoder APIs create, configure, decode, and release successfully\n";
} catch (Throwable $e) {
    echo "not ok 1 - raw decoder lifecycle check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $decoder !== null) {
        try {
            $ffi->sixel_decoder_unref($decoder);
        } catch (Throwable $e) {
        }
    }
}
