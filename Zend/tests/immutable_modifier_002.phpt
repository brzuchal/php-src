--TEST--
Immutable modifier on abstract class.
--FILE--
<?php

abstract immutable class Foo {}

$foo = new Foo();
?>
--EXPECTF--

Fatal error: Cannot use the immutable modifier on an abstract class in %simmutable_modifier_002.php on line 3