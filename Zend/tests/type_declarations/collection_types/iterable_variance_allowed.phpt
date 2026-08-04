--TEST--
F2 OQ-1: collection descriptors are subtypes of iterable (allowed overrides)
--FILE--
<?php
// Covariant return: a collection narrows an iterable return.
class A { public function f(): iterable { return vec[int]{1}; } }
class B extends A { public function f(): vec[int] { return vec[int]{1}; } }
class C extends A { public function f(): vec[] { return vec[int]{1}; } }
class Ds { public function g(): iterable { return set[int]{1}; } }
class Es extends Ds { public function g(): set[] { return set[int]{1}; } }
class Ft { public function h(): iterable { return tuple[int]{1}; } }
class Gt extends Ft { public function h(): tuple[] { return tuple[int]{1}; } }
// Nullable narrowing: vec[] narrows ?iterable.
class N { public function f(): ?iterable { return null; } }
class M extends N { public function f(): vec[] { return vec[int]{1}; } }

// Contravariant parameter: iterable widens a collection parameter.
class P { public function m(vec[] $x): void {} }
class Q extends P { public function m(iterable $x): void {} }
class Pi { public function m(vec[int] $x): void {} }
class Qi extends Pi { public function m(iterable $x): void {} }

echo "ok\n";
?>
--EXPECT--
ok
