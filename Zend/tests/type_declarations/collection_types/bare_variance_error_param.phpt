--TEST--
Bare collection-kind types: parameter narrowing (bare -> concrete) is rejected
--FILE--
<?php
class A { public function set(vec[] $v): void {} }
class B extends A { public function set(vec[int] $v): void {} }
?>
--EXPECTF--
Fatal error: Declaration of B::set(vec[int] $v): void must be compatible with A::set(vec[] $v): void in %s on line %d
