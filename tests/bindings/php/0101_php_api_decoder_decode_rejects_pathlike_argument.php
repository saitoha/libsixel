#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$decoder = null;

try {
    $pathlike = new class {
        public function __toString(): string
        {
            return (string) getenv('TOP_SRCDIR') . '/tests/data/inputs/snake_64.six';
        }
    };

    $decoder = new Decoder();

    try {
        $decoder->decode($pathlike);
        echo "not ok 1 - decoder decode accepted unexpected path-like argument\n";
    } catch (TypeError $e) {
        echo "ok 1 - decoder decode rejects unexpected path-like argument\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - decoder decode path-like rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        $decoder->close();
    }
}
