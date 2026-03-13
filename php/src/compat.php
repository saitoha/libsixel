<?php
/**
 * Backward-compatible global class aliases.
 *
 * @license MIT
 */

declare(strict_types=1);

if (!class_exists('SixelEncoder', false)) {
    class_alias(\Libsixel\Encoder::class, 'SixelEncoder');
}

if (!class_exists('SixelDecoder', false)) {
    class_alias(\Libsixel\Decoder::class, 'SixelDecoder');
}
