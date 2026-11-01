#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');
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

    $pixels = $ffi->new('unsigned char[8]');
    $pixels[0] = 0;
    $pixels[1] = 1;
    $pixels[2] = 2;
    $pixels[3] = 3;
    $pixels[4] = 0;
    $pixels[5] = 1;
    $pixels[6] = 2;
    $pixels[7] = 3;

    $palette = pack('C*',
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255
    );
    $paletteBytes = array_values(unpack('C*', $palette));
    $paletteArray = $ffi->new('unsigned char[' . count($paletteBytes) . ']');
    $paletteArray[0] = $paletteBytes[0];
    $paletteArray[1] = $paletteBytes[1];
    $paletteArray[2] = $paletteBytes[2];
    $paletteArray[3] = $paletteBytes[3];
    $paletteArray[4] = $paletteBytes[4];
    $paletteArray[5] = $paletteBytes[5];
    $paletteArray[6] = $paletteBytes[6];
    $paletteArray[7] = $paletteBytes[7];
    $paletteArray[8] = $paletteBytes[8];
    $paletteArray[9] = $paletteBytes[9];
    $paletteArray[10] = $paletteBytes[10];
    $paletteArray[11] = $paletteBytes[11];

    $status = (int)$ffi->sixel_encoder_encode_bytes($encoder, $pixels, 4, 2, 0x83, $paletteArray, count($paletteBytes));

    $ffi->sixel_encoder_unref($encoder);
    $encoder = null;

    if (API::failed($status)) {
        echo "not ok 1 - encoder encode_bytes rejected bytes palette payload\n";
    } else {
        echo "ok 1 - encoder encode_bytes accepts bytes palette payload\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder encode_bytes bytes palette acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $encoder !== null) {
        try {
            $ffi->sixel_encoder_unref($encoder);
        } catch (Throwable $e) {
        }
    }
}
