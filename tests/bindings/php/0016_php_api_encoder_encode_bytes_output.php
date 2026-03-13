#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;
use Libsixel\Constants;

echo "1..1\n";

$bindingRoot = getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = getenv('LIBSIXEL_LIBPATH');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';

if (!is_string($bindingRoot) || $bindingRoot === '' || !is_string($libPath) || $libPath === '') {
    echo "not ok 1 - required test environment variables are missing\n";
    exit(1);
}

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_encoder sixel_encoder_t;

SIXELSTATUS sixel_encoder_new(sixel_encoder_t **ppencoder, void *allocator);
void sixel_encoder_unref(sixel_encoder_t *encoder);
SIXELSTATUS sixel_encoder_setopt(sixel_encoder_t *encoder, int arg, const char *value);
SIXELSTATUS sixel_encoder_encode_bytes(
    sixel_encoder_t *encoder,
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char *palette,
    int ncolors
);
CDEF, $libPath);

    $encoderOut = $ffi->new('sixel_encoder_t *[1]');
    $status = (int)$ffi->sixel_encoder_new($encoderOut, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_encoder_new failed with status ' . $status);
    }
    $encoder = $encoderOut[0];

    $status = (int)$ffi->sixel_encoder_setopt($encoder, ord(Constants::SIXEL_OPTFLAG_OUTPUT), $devNull);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_encoder_setopt failed with status ' . $status);
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

    $status = (int)$ffi->sixel_encoder_encode_bytes($encoder, $pixels, 2, 2, 3, null, 0);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_encoder_encode_bytes failed with status ' . $status);
    }

    $ffi->sixel_encoder_unref($encoder);
    $encoder = null;

    echo "ok 1 - encoder encode_bytes accepts pixel buffer and configured output\n";
} catch (Throwable $e) {
    echo "not ok 1 - encoder encode_bytes output check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        try {
            $ffi->sixel_encoder_unref($encoder);
        } catch (Throwable $e) {
        }
    }
}
