#!/usr/bin/env php
<?php

declare(strict_types=1);

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');


require_once $bindingRoot . '/src/autoload.php';

try {
    $ffi = FFI::cdef(<<<'CDEF'
const char *sixel_helper_format_error(int status);
CDEF, $libPath);

    $raw = $ffi->sixel_helper_format_error(0x1100);
    if ($raw === null) {
        throw new RuntimeException('sixel_helper_format_error returned null');
    }

    $message = is_string($raw) ? $raw : FFI::string($raw);
    if (!is_string($message) || $message === '') {
        echo "not ok 1 - helper format_error returned an empty message\n";
    } else {
        echo "ok 1 - helper format_error returned a non-empty message\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - helper format_error check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
