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
void sixel_output_ref(sixel_output_t *output);
void sixel_output_unref(sixel_output_t *output);
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
    $ffi->sixel_output_ref($output);
    $ffi->sixel_output_unref($output);
    $ffi->sixel_output_unref($output);
    $output = null;

    echo "ok 1 - output lifecycle APIs are callable\n";
} catch (Throwable $e) {
    echo "not ok 1 - output lifecycle API check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $output !== null) {
        try {
            $ffi->sixel_output_unref($output);
        } catch (Throwable $e) {
        }
    }
}
