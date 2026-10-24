#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $bytearrayAll = [97, 108, 108];

    try {
        API::setEncoderThreads($bytearrayAll);
        echo "not ok 1 - set_encoder_threads accepted bytearray-like all input\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - set_encoder_threads rejects bytearray-like all input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_encoder_threads bytearray-like all rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
