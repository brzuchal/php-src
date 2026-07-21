--TEST--
collection types: head name must be unqualified
--FILE--
<?php
function f(\Ns\vec[int] $x) {}
?>
--EXPECTF--
Fatal error: Collection type "Ns\vec" must be unqualified in %s on line %d
