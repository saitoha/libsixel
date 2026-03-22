#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    API::setThreads('auto');

    echo "ok 1 - set_threads accepts auto keyword as bytes-like input\n";
} catch (Throwable $e) {
    echo "not ok 1 - set_threads auto-byte acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
