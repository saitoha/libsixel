#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;

try {
    $encoder = new Encoder();

    $bytesDirectory = pack('C*', ord('.'));

    try {
        $encoder->encode($bytesDirectory);
        echo "not ok 1 - encoder accepted directory bytes filename input\n";
    } catch (Throwable $e) {
        echo "ok 1 - encoder encode rejects directory bytes filename input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder directory bytes filename rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
