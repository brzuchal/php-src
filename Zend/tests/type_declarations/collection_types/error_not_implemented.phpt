--TEST--
collection types: the parser accepts the whole family, the compiler gates it
--FILE--
<?php
function f(map[string, int] $x) {}
?>
--EXPECTF--
Fatal error: Collection type map is not implemented yet in %s on line %d
