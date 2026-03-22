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

    try {
        $decoder->decode(12345);
        echo "not ok 1 - decoder accepted integer infile input\n";
    } catch (TypeError $e) {
        echo "ok 1 - decoder decode rejects integer infile input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - decoder integer infile rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        $decoder->close();
    }
}
