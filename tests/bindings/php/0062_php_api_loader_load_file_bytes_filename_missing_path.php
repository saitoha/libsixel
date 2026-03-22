#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');

require_once $bindingRoot . '/src/autoload.php';

$ffi = null;
$loader = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_loader sixel_loader_t;
typedef struct sixel_frame sixel_frame_t;
typedef int (*sixel_load_image_function)(sixel_frame_t *frame, void *context);
SIXELSTATUS sixel_loader_new(sixel_loader_t **pploader, void *allocator);
void sixel_loader_unref(sixel_loader_t *loader);
SIXELSTATUS sixel_loader_load_file(sixel_loader_t *loader, char const *filename, sixel_load_image_function fn_load);
CDEF, $libPath);

    $out = $ffi->new('sixel_loader_t *[1]');
    $status = (int)$ffi->sixel_loader_new($out, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_loader_new failed: ' . $status);
    }

    $loader = $out[0];
    $missingPath = hex2bin('2f646566696e6974656c792f6d697373696e672f696d6167652e706e67');
    if (!is_string($missingPath)) {
        throw new RuntimeException('failed to build bytes-like filename payload');
    }

    $loadCallback = function ($frame, $context): int {
        return 0;
    };

    $status = (int)$ffi->sixel_loader_load_file($loader, $missingPath, $loadCallback);

    $ffi->sixel_loader_unref($loader);
    $loader = null;

    if (API::failed($status)) {
        echo "ok 1 - loader load_file handles bytes-like filename on missing path\n";
    } else {
        echo "not ok 1 - loader load_file unexpectedly succeeded on missing bytes-like path\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - loader bytes-like filename missing-path check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $loader !== null) {
        try {
            $ffi->sixel_loader_unref($loader);
        } catch (Throwable $e) {
        }
    }
}
