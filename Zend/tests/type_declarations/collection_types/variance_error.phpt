--TEST--
collection types: a different parameter type is not compatible
--FILE--
<?php
class P { public function m(vec[int] $a): void {} }
class Q extends P { public function m(vec[string] $a): void {} }
?>
--EXPECTF--
Fatal error: Declaration of Q::m(vec[string] $a): void must be compatible with P::m(vec[int] $a): void in %s on line %d
