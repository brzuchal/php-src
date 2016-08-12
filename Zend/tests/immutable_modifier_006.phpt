--TEST--
Clone instance of immutable class.
--FILE--
<?php

immutable class Foo {}

$foo = new Foo();
$clone = clone $foo;
?>
--EXPECTF--

Fatal error: Uncaught Error: Cloning instance of immutable class (Foo) is not allowed in %simmutable_modifier_006.php:6
Stack trace:
#0 {main}
  thrown in %simmutable_modifier_006.php on line 6