#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;

try {
    $encoder = new Encoder();
    $encoder->setopt(
        Constants::SIXEL_OPTFLAG_QUANTIZE_MODEL,
        'auto:sampling_policy=adaptive-grid:binning_policy=hard'
    );
    $encoder->setopt(
        Constants::SIXEL_OPTFLAG_BACKGROUND_POLICY,
        'explicit_first'
    );

    echo "ok 1 - encoder accepts nested palette policy suboptions\n";
} catch (Throwable $e) {
    echo "not ok 1 - encoder nested policy suboption check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
