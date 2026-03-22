#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $value = hex2bin('203220');
    if (!is_string($value)) {
        throw new RuntimeException('failed to build whitespace-padded numeric bytes-like payload');
    }

    API::setThreads($value);

    echo "ok 1 - set_threads accepts whitespace-padded numeric bytes-like input\n";
} catch (Throwable $e) {
    echo "not ok 1 - set_threads whitespace-padded numeric bytes acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
