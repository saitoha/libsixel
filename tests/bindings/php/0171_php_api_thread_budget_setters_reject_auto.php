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
        foreach (['auto', 'AUTO', ' AuTo '] as $value) {
            try {
                API::$setter($value);
                throw new RuntimeException('thread budget API accepted auto');
            } catch (InvalidArgumentException $e) {
                if ($e->getMessage() !== "threads must be a positive integer or 'all'") {
                    throw $e;
                }
            }
        }
    }
    echo "ok 1 - both thread budget setters reject auto\n";
} catch (Throwable $e) {
    echo "not ok 1 - thread budget auto rejection failed\n";
    echo '# ' . get_class($e) . ': '
        . preg_replace('/\s+/', ' ', $e->getMessage()) . "\n";
}
