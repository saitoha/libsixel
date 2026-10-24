#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    API::setEncoderThreads('2');
    API::setEncoderThreads('auto');

    try {
        API::setEncoderThreads('bad');
        echo "not ok 1 - set_encoder_threads accepted invalid bytes-like input\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - set_encoder_threads accepts valid bytes-like inputs and rejects invalid input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_encoder_threads bytes-like input check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
