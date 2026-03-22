#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Decoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$sourceRoot = (string) getenv('TOP_SRCDIR');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';

require_once $bindingRoot . '/src/autoload.php';

$decoder = null;

try {
    $source = $sourceRoot . '/tests/data/inputs/snake_64.six';

    $decoder = new Decoder();
    $decoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
    $decoder->decode($source);

    echo "ok 1 - decoder decode infile-argument path is handled\n";
} catch (Throwable $e) {
    echo "not ok 1 - decoder decode infile-argument check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        $decoder->close();
    }
}
