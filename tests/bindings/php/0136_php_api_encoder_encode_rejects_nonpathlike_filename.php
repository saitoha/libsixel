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

    $rejected = false;
    try {
        $encoder->encode(new stdClass());
    } catch (Throwable $e) {
        $rejected = true;
    }

    if ($rejected) {
        echo "ok 1 - encoder encode rejects non-pathlike filename object\n";
    } else {
        echo "not ok 1 - encoder encode accepted non-pathlike filename object\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - encoder non-pathlike filename rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
