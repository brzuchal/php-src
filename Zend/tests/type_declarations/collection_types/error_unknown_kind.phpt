--TEST--
collection types: unknown collection kind
--FILE--
<?php
function f(Foo[int] $x) {}
?>
--EXPECTF--
Fatal error: Unknown collection type "Foo" in %s on line %d
