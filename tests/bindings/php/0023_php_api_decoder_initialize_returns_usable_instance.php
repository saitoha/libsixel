#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = getenv('SIXEL_TEST_PHP_BINDING_ROOT');

if (!is_string($bindingRoot) || $bindingRoot === '') {
    echo "not ok 1 - SIXEL_TEST_PHP_BINDING_ROOT is not set\n";
    exit(1);
}

require_once $bindingRoot . '/src/autoload.php';

try {
    $decoder = new Decoder();
    if ($decoder instanceof Decoder) {
        echo "ok 1 - decoder initialize returns usable decoder instance\n";
    } else {
        echo "not ok 1 - decoder initialize did not return Decoder instance\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - decoder initialize usability check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}
