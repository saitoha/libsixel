#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';

require_once $bindingRoot . '/src/autoload.php';

$decoder = null;

try {
    $decoder = new Decoder();
    $decoder->setopt(ord('o'), $devNull);

    echo "ok 1 - decoder accepts integer option flag via wrapper\n";
} catch (Throwable $e) {
    echo "not ok 1 - decoder integer option flag acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        $decoder->close();
    }
}
