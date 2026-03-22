#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    try {
        API::setThreads('   ');
        echo "not ok 1 - set_threads accepted whitespace-only string input\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - set_threads rejects whitespace-only string input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_threads whitespace-only string path check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
