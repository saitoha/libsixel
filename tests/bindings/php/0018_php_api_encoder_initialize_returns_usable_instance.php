#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = getenv('SIXEL_TEST_PHP_BINDING_ROOT');

if (!is_string($bindingRoot) || $bindingRoot === '') {
    echo "not ok 1 - SIXEL_TEST_PHP_BINDING_ROOT is not set\n";
    exit(1);
}

require_once $bindingRoot . '/src/autoload.php';

try {
    $encoder = new Encoder();
    if ($encoder instanceof Encoder) {
        echo "ok 1 - encoder initialize returns usable encoder instance\n";
    } else {
        echo "not ok 1 - encoder initialize did not return Encoder instance\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder initialize usability check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}
