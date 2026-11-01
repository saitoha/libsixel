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
    $memoryviewLike = new ArrayObject(['dummy.png']);

    try {
        $encoder->encode($memoryviewLike);
        echo "not ok 1 - encoder encode accepted memoryview-like filename input\n";
    } catch (TypeError $e) {
        echo "ok 1 - encoder encode rejects memoryview-like filename input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder memoryview-like filename rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
