--TEST--
SixelEncoder::__construct() member function
--SKIPIF--
<?php 

if(!extension_loaded('sixel')) die('skip ');

 ?>
--FILE--
<?php
echo 'OK'; // no test case for this function yet
?>
--EXPECT--
OK