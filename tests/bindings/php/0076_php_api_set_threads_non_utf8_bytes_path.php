#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $value = hex2bin('ff');
    if (!is_string($value)) {
        throw new RuntimeException('failed to build non-UTF-8 bytes-like payload');
    }

    try {
        API::setThreads($value);
        echo "not ok 1 - set_threads accepted non-UTF-8 bytes-like input\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - set_threads rejects non-UTF-8 bytes-like input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_threads non-UTF-8 bytes path check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
