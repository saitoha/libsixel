#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

function tap_fail(int $n, string $message): void
{
    $message = preg_replace('/\s+/', ' ', $message);
    if (!is_string($message) || $message === '') {
        $message = 'unknown failure';
    }
    echo "not ok {$n} - {$message}\n";
    exit(1);
}

echo "1..3\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libDir = (string) getenv('LIBSIXEL_LIBDIR');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');

$resolvedBindingRoot = realpath($bindingRoot);
$resolvedLibDir = realpath($libDir);
$resolvedLibPath = realpath($libPath);

if (!is_string($resolvedBindingRoot) || !is_string($resolvedLibDir) || !is_string($resolvedLibPath)) {
    tap_fail(1, 'failed to resolve packaged binding and library paths');
}

if (strpos($resolvedLibDir, $resolvedBindingRoot . DIRECTORY_SEPARATOR) !== 0
    || strpos($resolvedLibPath, $resolvedBindingRoot . DIRECTORY_SEPARATOR) !== 0
) {
    tap_fail(1, 'php binding did not resolve to packaged libsixel path');
}

require_once $resolvedBindingRoot . '/src/autoload.php';
echo "ok 1 - packaged php binding paths are configured\n";

try {
    $encoder = new Encoder();
    echo "ok 2 - encoder constructed\n";
} catch (Throwable $e) {
    tap_fail(2, 'encoder construction failed: ' . $e->getMessage());
}

try {
    $artifactLocalDir = (string) getenv('ARTIFACT_LOCAL_DIR');
    if ($artifactLocalDir === '') {
        $artifactLocalDir = sys_get_temp_dir() . '/libsixel-php-artifacts';
    }
    if (!is_dir($artifactLocalDir) && !mkdir($artifactLocalDir, 0777, true)) {
        throw new RuntimeException('failed to create artifact directory: ' . $artifactLocalDir);
    }

    $sourceRoot = (string) getenv('TOP_SRCDIR');

    $source = $sourceRoot . '/tests/data/inputs/snake_64.png';
    $output = $artifactLocalDir . '/php_bindings_smoke.six';

    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $output);
    $encoder->setopt(Constants::SIXEL_OPTFLAG_COLORS, '16');
    $encoder->encode($source);
    $encoder->close();

    if (!is_file($output)) {
        throw new RuntimeException('sixel output was not generated');
    }

    $payload = file_get_contents($output);
    if (!is_string($payload) || $payload === '') {
        throw new RuntimeException('encoded output is empty');
    }

    $trimmed = preg_replace('/[\r\n]+\z/', '', $payload);
    if (!is_string($trimmed)
        || strpos($trimmed, "\x1bPq") !== 0
        || substr($trimmed, -2) !== "\x1b\\"
    ) {
        throw new RuntimeException('encoded output does not contain a valid sixel envelope');
    }

    echo "ok 3 - encode output generated from packaged binding\n";
} catch (Throwable $e) {
    tap_fail(3, $e->getMessage());
}
