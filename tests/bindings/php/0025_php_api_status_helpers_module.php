#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;
use Libsixel\Constants;

echo "1..1\n";

$bindingRoot = getenv('SIXEL_TEST_PHP_BINDING_ROOT');

if (!is_string($bindingRoot) || $bindingRoot === '') {
    echo "not ok 1 - SIXEL_TEST_PHP_BINDING_ROOT is not set\n";
    exit(1);
}

require_once $bindingRoot . '/src/autoload.php';

try {
    $statuses = [
        [Constants::SIXEL_FALSE, false],
        [Constants::SIXEL_RUNTIME_ERROR, false],
        [Constants::SIXEL_BAD_ALLOCATION, false],
    ];

    $valid = true;
    foreach ($statuses as $status) {
        $succeeded = API::succeeded($status[0]);
        $failed = API::failed($status[0]);
        if ($succeeded === $failed || $succeeded !== $status[1] || $failed === $status[1]) {
            $valid = false;
            break;
        }
    }

    if ($valid) {
        echo "ok 1 - status helper predicates classify success/failure consistently\n";
    } else {
        echo "not ok 1 - status helper predicates returned contradictory results\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - status helper predicate check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}
