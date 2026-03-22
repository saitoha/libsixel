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
    $missing = '/path/which/does/not/exist.png';
    $bytesMissing = pack('C*', ...array_values(unpack('C*', $missing)));

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);

    try {
        $encoder->encode($bytesMissing);
        echo "not ok 1 - encoder accepted missing bytes path input\n";
    } catch (Throwable $e) {
        echo "ok 1 - encoder rejects missing bytes path input\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder missing bytes path rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
