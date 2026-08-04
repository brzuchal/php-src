--TEST--
collection types: not allowed inside a union type
--FILE--
<?php
function f(vec[int]|Countable $x) {}
?>
--EXPECTF--
Fatal error: Collection type cannot be part of a union type; write ?vec[...] for a nullable collection in %s on line %d
