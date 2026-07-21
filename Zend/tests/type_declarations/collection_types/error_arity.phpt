--TEST--
collection types: wrong number of parameters
--FILE--
<?php
function f(vec[int, string] $x) {}
?>
--EXPECTF--
Fatal error: Collection type vec expects 1 parameter, 2 given in %s on line %d
