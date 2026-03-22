#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');


require_once $bindingRoot . '/src/autoload.php';

try {
    API::setThreads(1);
    API::setThreads('auto');

    try {
        API::setThreads('invalid');
        echo "not ok 1 - set_threads accepted invalid input\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - set_threads accepts valid inputs and rejects invalid input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_threads API check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
