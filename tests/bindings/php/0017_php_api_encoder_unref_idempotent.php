#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';

if (!is_string($bindingRoot) || $bindingRoot === '') {
    echo "not ok 1 - SIXEL_TEST_PHP_BINDING_ROOT is not set\n";
    exit(1);
}

require_once $bindingRoot . '/src/autoload.php';

try {
    $encoder = new Encoder();
    $encoder->close();
    $encoder->close();

    try {
        $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
        echo "not ok 1 - encoder accepted setopt after close\n";
    } catch (Throwable $e) {
        $message = strtolower($e->getMessage());
        if (strpos($message, 'closed') !== false) {
            echo "ok 1 - encoder unref path is idempotent\n";
        } else {
            echo "not ok 1 - closed encoder did not report closed state\n";
            echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
        }
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder unref idempotent check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}
