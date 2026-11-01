#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';


require_once $bindingRoot . '/src/autoload.php';

try {
    $decoder = new Decoder();
    $decoder->close();
    $decoder->close();

    try {
        $decoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
        echo "not ok 1 - decoder accepted setopt after close\n";
    } catch (Throwable $e) {
        $message = strtolower($e->getMessage());
        if (strpos($message, 'closed') !== false) {
            echo "ok 1 - decoder unref path is idempotent\n";
        } else {
            echo "not ok 1 - closed decoder did not report closed state\n";
            echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
        }
    }
} catch (Throwable $e) {
    echo "not ok 1 - decoder unref idempotent check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}
