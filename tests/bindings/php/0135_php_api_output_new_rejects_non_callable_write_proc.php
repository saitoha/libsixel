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
CDEF, $libPath);

    $out = $ffi->new('sixel_output_t *[1]');

    $rejected = false;
    try {
        $status = (int)$ffi->sixel_output_new($out, 123, null, null);
        $rejected = API::failed($status);
        if (!$rejected && $out[0] !== null && !FFI::isNull($out[0])) {
            $output = $out[0];
        }
    } catch (Throwable $e) {
        $rejected = true;
    }

    if ($output !== null) {
        $ffi->sixel_output_unref($output);
        $output = null;
    }

    if ($rejected) {
        echo "ok 1 - output_new rejects non-callable write_proc\n";
    } else {
        echo "not ok 1 - output_new accepted non-callable write_proc\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - output_new non-callable write_proc rejection check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $output !== null) {
        try {
            $ffi->sixel_output_unref($output);
        } catch (Throwable $e) {
        }
    }
}
