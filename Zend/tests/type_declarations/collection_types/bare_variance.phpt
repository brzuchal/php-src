--TEST--
Bare collection-kind types: allowed variance (a bare kind is the top of its kind)
--FILE--
<?php
// covariant return: narrow bare -> concrete
class A { public function get(): vec[] { return vec[int]{1}; } }
class B extends A { public function get(): vec[int] { return vec[int]{1}; } }
echo "return narrow ok\n";

// contravariant parameter: widen concrete -> bare
class C { public function set(vec[int] $v): void {} }
class D extends C { public function set(vec[] $v): void {} }
echo "param widen ok\n";

// bare -> bare, same kind
class E { public function get(): vec[] { return vec[int]{1}; } }
class F extends E { public function get(): vec[] { return vec[int]{1}; } }
echo "bare->bare ok\n";
?>
--EXPECT--
return narrow ok
param widen ok
bare->bare ok
