--TEST--
F2: nullable-iterable variance -- collections narrow ?iterable / iterable|null (allowed directions)
--FILE--
<?php
// Covariant return: a collection narrows nullable iterable.
class A { public function f(): ?iterable { return null; } }
class B extends A { public function f(): vec[] { return vec[int]{1}; } }
class C { public function f(): iterable|null { return null; } }
class D extends C { public function f(): set[] { return set[int]{1}; } }
// Nullable narrows nullable.
class E { public function f(): ?iterable { return null; } }
class F extends E { public function f(): ?vec[] { return null; } }

// Contravariant parameter: nullable iterable widens a collection parameter.
class P { public function m(vec[] $x): void {} }
class Q extends P { public function m(iterable|null $x): void {} }
class R { public function m(tuple[] $x): void {} }
class S extends R { public function m(?iterable $x): void {} }

echo "ok\n";
?>
--EXPECT--
ok
