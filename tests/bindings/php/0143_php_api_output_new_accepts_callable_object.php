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

    $callable = new class {
        public function __invoke($data, $size, $priv): int
        {
            return 0;
        }
    };

    $out = $ffi->new('sixel_output_t *[1]');
    $status = (int)$ffi->sixel_output_new($out, $callable, null, null);
    if (!API::failed($status) && $out[0] !== null && !FFI::isNull($out[0])) {
        $output = $out[0];
    }

    if ($output !== null) {
        $ffi->sixel_output_unref($output);
        $output = null;
        echo "ok 1 - output_new accepts callable-object write_proc\n";
    } else {
        echo "not ok 1 - output_new rejected callable-object write_proc\n";
    }
} catch (Throwable $e) {
    echo "not ok 1 - output_new callable-object acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($ffi !== null && $output !== null) {
        try {
            $ffi->sixel_output_unref($output);
        } catch (Throwable $e) {
        }
    }
}
