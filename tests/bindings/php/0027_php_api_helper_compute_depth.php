#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');


require_once $bindingRoot . '/src/autoload.php';

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
int sixel_helper_compute_depth(int pixelformat);
CDEF, $libPath);

    $depth = (int)$ffi->sixel_helper_compute_depth(0x03);
    if ($depth === 3) {
        echo "ok 1 - helper compute_depth returns expected value for RGB888\n";
    } else {
        echo "not ok 1 - unexpected depth for RGB888: {$depth}\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - helper compute_depth check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
