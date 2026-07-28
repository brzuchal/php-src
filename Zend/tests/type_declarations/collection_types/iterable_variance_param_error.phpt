--TEST--
F2 OQ-1: a collection parameter cannot override an iterable parameter (narrowing)
--FILE--
<?php
class A { public function f(iterable $x) {} }
class B extends A { public function f(vec[] $x) {} }
?>
--EXPECTF--
Fatal error: Declaration of B::f(vec[] $x) must be compatible with A::f(Traversable|array $x) in %s on line %d
