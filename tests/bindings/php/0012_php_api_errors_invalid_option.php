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
    $encoder->setopt('X', '16');
    echo "not ok 1 - encoder accepted unsupported option flag\n";
} catch (Throwable $e) {
    echo "ok 1 - encoder rejects unsupported option flag\n";
}
