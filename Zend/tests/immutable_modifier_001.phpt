--TEST--
Multiple immutable modifiers.
--FILE--
<?php

immutable immutable class Foo {}

$foo = new Foo();
?>
--EXPECTF--

Fatal error: Multiple immutable modifiers are not allowed in %simmutable_modifier_001.php on line 3