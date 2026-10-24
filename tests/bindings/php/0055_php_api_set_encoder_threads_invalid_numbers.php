#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $rejectedZero = false;
    $rejectedNegative = false;

    try {
        API::setEncoderThreads(0);
    } catch (InvalidArgumentException $e) {
        $rejectedZero = true;
    }

    try {
        API::setEncoderThreads(-1);
    } catch (InvalidArgumentException $e) {
        $rejectedNegative = true;
    }

    API::setEncoderThreads(1);

    if ($rejectedZero && $rejectedNegative) {
        echo "ok 1 - set_encoder_threads rejects zero and negative numeric inputs\n";
    } else {
        echo "not ok 1 - set_encoder_threads numeric validation path was unexpected\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_encoder_threads numeric validation check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
