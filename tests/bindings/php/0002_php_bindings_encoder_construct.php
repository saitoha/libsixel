#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$resolvedBindingRoot = realpath($bindingRoot);
if (!is_string($resolvedBindingRoot)) {
    echo "not ok 1 - failed to resolve php binding root\n";
    exit(0);
}

require_once $resolvedBindingRoot . '/src/autoload.php';

try {
    new Encoder();
    echo "ok 1 - encoder constructed\n";
} catch (Throwable $e) {
    $message = preg_replace('/\s+/', ' ', $e->getMessage());
    if (!is_string($message) || $message === '') {
        $message = 'unknown failure';
    }
    echo "not ok 1 - encoder construction failed: {$message}\n";
}
