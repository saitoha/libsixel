#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Decoder;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;
$decoder = null;

try {
    $encoder = new Encoder();
    $decoder = new Decoder();

    if ($encoder instanceof Encoder && $decoder instanceof Decoder) {
        echo "ok 1 - php bindings smoke lifecycle verified\n";
    } else {
        echo "not ok 1 - php bindings smoke lifecycle verification failed\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - php bindings smoke lifecycle check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        $decoder->close();
    }
    if ($encoder !== null) {
        $encoder->close();
    }
}
