#!/usr/bin/env php
<?php

declare(strict_types=1);

use Libsixel\Constants;
use Libsixel\Encoder;

echo "1..1\n";

$bindingRoot = (string) getenv('SIXEL_TEST_PHP_BINDING_ROOT');
$sourceRoot = (string) getenv('TOP_SRCDIR');
$devNull = DIRECTORY_SEPARATOR === '\\' ? 'NUL' : '/dev/null';

require_once $bindingRoot . '/src/autoload.php';

$encoder = null;

try {
    $source = $sourceRoot . '/tests/data/inputs/snake_64.png';
    $pathlike = new class($source) {
        private string $path;

        public function __construct(string $path)
        {
            $this->path = $path;
        }

        public function __toString(): string
        {
            return $this->path;
        }
    };

    $encoder = new Encoder();
    $encoder->setopt(Constants::SIXEL_OPTFLAG_OUTPUT, $devNull);
    $encoder->encode((string)$pathlike);

    echo "ok 1 - encoder accepts path-like input through string conversion\n";
} catch (Throwable $e) {
    echo "not ok 1 - encoder path-like input acceptance check failed\n";
    echo '# ' . get_class($e) . ': ' . preg_replace('/\\s+/', ' ', $e->getMessage()) . "\n";
} finally {
    if ($encoder !== null) {
        $encoder->close();
    }
}
