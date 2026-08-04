--TEST--
F2 OQ-1: an iterable return cannot override a collection return (widening)
--FILE--
<?php
class A { public function f(): vec[] { return vec[int]{1}; } }
class B extends A { public function f(): iterable { return []; } }
?>
--EXPECTF--
Fatal error: Declaration of B::f(): Traversable|array must be compatible with A::f(): vec[] in %s on line %d
