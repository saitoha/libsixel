#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$decoder = null;

try {
    $decoder = new Decoder();

    $bytearrayFlag = [105];

    try {
        $decoder->setopt($bytearrayFlag, 'dummy.png');
        echo "not ok 1 - decoder accepted bytearray-like option flag input\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - decoder setopt rejects bytearray-like option flag input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - decoder bytearray-like option flag rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        $decoder->close();
    }
}
