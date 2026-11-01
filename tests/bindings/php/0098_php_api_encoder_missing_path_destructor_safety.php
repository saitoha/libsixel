#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

try {
    $encoder = new Encoder();
    $rejected = false;

    try {
        $encoder->encode('tests/data/inputs/formats/this_file_does_not_exist.png');
    } catch (Throwable $e) {
        $rejected = true;
    }

    if ($rejected) {
        $encoder = null;
        gc_collect_cycles();
        echo "ok 1 - encoder destructor stayed stable after missing-path encode failure\n";
    } else {
        echo "not ok 1 - encoder accepted missing input path\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder missing-path destructor safety check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
