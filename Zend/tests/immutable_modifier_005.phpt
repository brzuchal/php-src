--TEST--
Non immutable class can not extend from immutable one.
--FILE--
<?php

immutable class Foo {}
class Bar extends Foo {}

$bar = new Bar();
?>
--EXPECTF--

Fatal error: Immutable class Foo may not be extended by non immutable class Bar in %simmutable_modifier_005.php on line 4