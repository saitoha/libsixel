#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $bytearrayAuto = [97, 117, 116, 111];

    try {
        API::setThreads($bytearrayAuto);
        echo "not ok 1 - set_threads accepted bytearray-like auto input\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - set_threads rejects bytearray-like auto input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - set_threads bytearray-like auto rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
