#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$sourceRoot = (string) getenv('TOP_SRCDIR');

require_once $bindingRoot . '/src/autoload.php';

try {
    $header = file_get_contents($sourceRoot . '/include/sixel.h.in');
    if ($header === false) {
        throw new RuntimeException('failed to read include/sixel.h.in');
    }

    preg_match_all('/^#define\\s+(SIXEL_[A-Z0-9_]+)\\s+/m', $header, $matches);
    $expected = [];
    $prefixes = [
        'SIXEL_OPTFLAG_',
        'SIXEL_LOADER_OPTION_',
        'SIXEL_LUT_POLICY_',
        'SIXEL_COLORSPACE_',
    ];
    foreach ($matches[1] as $name) {
        foreach ($prefixes as $prefix) {
            if (strpos($name, $prefix) === 0) {
                $expected[$name] = true;
                break;
            }
        }
    }

    $reflection = new ReflectionClass(Constants::class);
    $missing = [];
    foreach (array_keys($expected) as $name) {
        if (!$reflection->hasConstant($name)) {
            $missing[] = $name;
        }
    }
    if ($missing !== []) {
        throw new RuntimeException('missing constants: ' . implode(', ', $missing));
    }

    $checks = [
        'SIXEL_LOADER_OPTION_START_FRAME_NO' => 11,
        'SIXEL_LUT_POLICY_RBC' => 0x9,
        'SIXEL_LUT_POLICY_MAHALANOBIS' => 0xa,
        'SIXEL_OPTFLAG_WORKING_COLORSPACE' => 'W',
    ];
    foreach ($checks as $name => $expectedValue) {
        $actual = $reflection->getConstant($name);
        if ($actual !== $expectedValue) {
            throw new RuntimeException(
                sprintf('%s value mismatch: got %s', $name, var_export($actual, true))
            );
        }
    }

    echo "ok 1 - setopt constants are synchronized with header\n";
} catch (Throwable $e) {
    echo "not ok 1 - setopt constant synchronization check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
