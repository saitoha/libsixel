#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$sourceRoot = (string) getenv('TOP_SRCDIR');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;

try {
    $source = $sourceRoot . '/tests/data/inputs/snake_64.png';
    $bytesSource = pack('C*', ...array_values(unpack('C*', $source)));

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
    $encoder->encode($bytesSource);

    echo "ok 1 - encoder accepts bytes filename input\n";
} catch (Throwable $e) {
    echo "not ok 1 - encoder bytes filename acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
