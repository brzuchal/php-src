--TEST--
collection types: only the five heads form a collection type
--FILE--
<?php
function f(Foo[int] $x) {}
?>
--EXPECTF--
Parse error: syntax error, unexpected token "[", expecting variable in %s on line %d
