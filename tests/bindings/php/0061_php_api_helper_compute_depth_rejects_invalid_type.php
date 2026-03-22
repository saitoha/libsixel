#!/usr/bin/env php
<?php

declare(strict_types=1);

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');

require_once $bindingRoot . '/src/autoload.php';

try {
    $ffi = FFI::cdef(<<<'CDEF'
int sixel_helper_compute_depth(int pixelformat);
CDEF, $libPath);

    $rejected = false;
    set_error_handler(static function (int $severity, string $message, string $file, int $line): bool {
        throw new ErrorException($message, 0, $severity, $file, $line);
    });
    try {
        $ffi->sixel_helper_compute_depth(new stdClass());
    } catch (Throwable $e) {
        $rejected = true;
    } finally {
        restore_error_handler();
    }

    if ($rejected) {
        echo "ok 1 - helper compute_depth rejects non-convertible argument type\n";
    } else {
        echo "not ok 1 - helper compute_depth accepted non-convertible argument type\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - helper compute_depth invalid-type check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
