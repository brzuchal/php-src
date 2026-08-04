--TEST--
Bare collection-kind types: return widening (concrete -> bare) is rejected
--FILE--
<?php
class A { public function get(): vec[int] { return vec[int]{1}; } }
class B extends A { public function get(): vec[] { return vec[int]{1}; } }
?>
--EXPECTF--
Fatal error: Declaration of B::get(): vec[] must be compatible with A::get(): vec[int] in %s on line %d
