#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $value = hex2bin('2020616c6c2020');
    if (!is_string($value)) {
        throw new RuntimeException('failed to build whitespace-padded all bytes-like payload');
    }

    API::setEncoderThreads($value);

    echo "ok 1 - set_encoder_threads accepts whitespace-padded all bytes-like input\n";
} catch (Throwable $e) {
    echo "not ok 1 - set_encoder_threads whitespace-padded all bytes acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
