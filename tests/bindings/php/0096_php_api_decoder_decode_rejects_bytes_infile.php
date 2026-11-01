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
    $bytesInfile = pack('C*', ...array_values(unpack('C*', '/path/which/does/not/exist.six')));

    $decoder = new Decoder();
    $decoder->setopt(Constants::SIXEL_OPTFLAG_INPUT, $source);
    $decoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);

    try {
        $decoder->decode($bytesInfile);
        echo "not ok 1 - decoder decode accepted bytes infile override\n";
    } catch (Throwable $e) {
        echo "ok 1 - decoder decode rejects bytes infile override in current PHP path\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - decoder decode bytes infile rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($decoder !== null) {
        $decoder->close();
    }
}
