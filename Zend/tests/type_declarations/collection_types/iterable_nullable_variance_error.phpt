--TEST--
F2: nullable-iterable variance -- a nullable collection cannot override a non-nullable iterable return (widens null)
--FILE--
<?php
class A { public function f(): iterable { return vec[int]{1}; } }
class B extends A { public function f(): ?vec[] { return null; } }
?>
--EXPECTF--
Fatal error: Declaration of B::f(): ?vec[] must be compatible with A::f(): Traversable|array in %s on line %d
