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

    try {
        $encoder->setopt(-1, '16');
        echo "not ok 1 - encoder accepted negative integer option flag\n";
    } catch (InvalidArgumentException $e) {
        echo "ok 1 - encoder rejects negative integer option flag\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder negative-flag rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
