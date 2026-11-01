#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Decoder;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$artifactLocalDir = (string) getenv('ARTIFACT_LOCAL_DIR');
$sourceRoot = (string) getenv('TOP_SRCDIR');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';


if (!is_string($artifactLocalDir) || $artifactLocalDir === '') {
    $artifactLocalDir = sys_get_temp_dir() . '/libsixel-php-artifacts';
}
if (!is_dir($artifactLocalDir) && !mkdir($artifactLocalDir, 0777, true) && !is_dir($artifactLocalDir)) {
    echo "not ok 1 - failed to create artifact directory\n";
    exit(1);
}

require_once $bindingRoot . '/src/autoload.php';

try {
    $source = $sourceRoot . '/tests/data/inputs/snake_64.png';
    $sixelPath = $artifactLocalDir . '/resource_roundtrip.six';

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $sixelPath);
    $encoder->setopt(Constants::SIXEL_OPTFLAG_WIDTH, '96');
    $encoder->setopt(Constants::SIXEL_OPTFLAG_HEIGHT, '72');
    $encoder->encode($source);

    if (!is_file($sixelPath) || filesize($sixelPath) === 0) {
        throw new RuntimeException('roundtrip sixel output is missing or empty');
    }

    $decoder = new Decoder();
    $decoder->setopt(Constants::SIXEL_OPTFLAG_INPUT, $sixelPath);
    $decoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
    $decoder->decode();

    echo "ok 1 - encode/decode roundtrip path is usable\n";
} catch (Throwable $e) {
    echo "not ok 1 - resource management roundtrip check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}
