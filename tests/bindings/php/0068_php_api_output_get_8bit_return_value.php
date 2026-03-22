#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\API;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$libPath = (string) getenv('LIBSIXEL_LIBPATH');

require_once $bindingRoot . '/src/autoload.php';

$ffi = null;
$output = null;

try {
    $ffi = FFI::cdef(<<<'CDEF'
typedef int SIXELSTATUS;
typedef struct sixel_output sixel_output_t;
typedef int (*sixel_write_function)(char *data, int size, void *priv);
SIXELSTATUS sixel_output_new(sixel_output_t **output, sixel_write_function fn_write, void *priv, void *allocator);
void sixel_output_unref(sixel_output_t *output);
void sixel_output_set_8bit_availability(sixel_output_t *output, int availability);
int sixel_output_get_8bit_availability(sixel_output_t *output);
CDEF, $libPath);

    $writeCallback = function ($data, $size, $priv): int {
        return 0;
    };

    $out = $ffi->new('sixel_output_t *[1]');
    $status = (int)$ffi->sixel_output_new($out, $writeCallback, null, null);
    if (API::failed($status)) {
        throw new RuntimeException('sixel_output_new failed: ' . $status);
    }

    $output = $out[0];
    $ffi->sixel_output_set_8bit_availability($output, 1);
    $result = (int)$ffi->sixel_output_get_8bit_availability($output);

    $ffi->sixel_output_unref($output);
    $output = null;

    if ($result === 0 || $result === 1) {
        echo "ok 1 - output get_8bit returned valid state ({$result})\n";
    } else {
        echo "not ok 1 - output get_8bit returned invalid state: {$result}\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - output get_8bit return value check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $output !== null) {
        try {
            $ffi->sixel_output_unref($output);
        } catch (Throwable $e) {
        }
    }
}
