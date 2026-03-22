#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $acceptedZero = true;
    $rejectedNegative = false;

    try {
        API::setThreads(0);
    } catch (InvalidArgumentException $e) {
        $acceptedZero = false;
    }

    try {
        API::setThreads(-1);
    } catch (InvalidArgumentException $e) {
        $rejectedNegative = true;
    }

    API::setThreads(1);

    if ($acceptedZero && $rejectedNegative) {
        echo "ok 1 - set_threads accepts zero-as-auto and rejects negative numeric input\n";
    } else {
        echo "not ok 1 - set_threads numeric validation path was unexpected\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_threads numeric validation check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
