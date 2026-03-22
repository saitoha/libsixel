#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    try {
        API::setThreads(null);
        echo "not ok 1 - set_threads accepted null input\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - set_threads rejects null input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_threads null validation check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
