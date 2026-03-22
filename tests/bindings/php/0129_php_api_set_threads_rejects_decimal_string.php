#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $rejected = false;

    try {
        API::setThreads('1.5');
    } catch (InvalidArgumentException $e) {
        $rejected = true;
    }

    API::setThreads(1);

    if ($rejected) {
        echo "ok 1 - set_threads rejects decimal-string input\n";
    } else {
        echo "not ok 1 - set_threads accepted decimal-string input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_threads decimal-string rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
