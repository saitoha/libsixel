#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;

try {
    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);

    try {
        $encoder->encode('tests/data/inputs/formats/this_file_does_not_exist.png');
        echo "not ok 1 - encoder accepted non-existing input file\n";
    } catch (Throwable $e) {
        echo "ok 1 - encoder rejects non-existing input file\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder missing file check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
