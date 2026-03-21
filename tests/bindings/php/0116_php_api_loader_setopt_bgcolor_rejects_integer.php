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
SIXELSTATUS sixel_loader_new(sixel_loader_t **pploader, void *allocator);
void sixel_loader_unref(sixel_loader_t *loader);
SIXELSTATUS sixel_loader_setopt(sixel_loader_t *loader, int option, void const *value);
CDEF, $libPath);

    $out = $ffi->new('sixel_loader_t *[1]');
    $status = (int)$ffi->sixel_loader_new($out, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_loader_new failed: ' . $status);
    }
    $loader = $out[0];

    $rejected = false;
    try {
        $status = (int)$ffi->sixel_loader_setopt($loader, 4, 1);
        $rejected = API::failed($status);
    } catch (Throwable $e) {
        $rejected = true;
    }

    $ffi->sixel_loader_unref($loader);
    $loader = null;

    if ($rejected) {
        echo "ok 1 - loader setopt rejects integer bgcolor input\n";
    } else {
        echo "not ok 1 - loader setopt accepted integer bgcolor input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - loader integer bgcolor rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $loader !== null) {
        try {
            $ffi->sixel_loader_unref($loader);
        } catch (Throwable $e) {
        }
    }
}
