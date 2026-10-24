#!/usr/bin/env php
<?php
# Policy: docs/cli/threading-policy.md

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');


require_once $bindingRoot . '/src/autoload.php';

try {
    foreach (['setEncoderThreads', 'setDecoderThreads'] as $setter) {
        foreach ([1, '2', 'all', ' ALL ', ' AlL '] as $value) {
            API::$setter($value);
        }
        foreach (['invalid', 0, -1, '0', '1.5', null] as $value) {
            try {
                API::$setter($value);
                throw new RuntimeException('thread budget API accepted invalid input');
            } catch (InvalidArgumentException $e) {
                // Each setter validates independently.
            }
        }
    }
    echo "ok 1 - both thread budget APIs validate inputs\n";
} catch (Throwable $e) {
    echo "not ok 1 - thread budget API check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
}
