--TEST--
F2 OQ-1: richer-union variance error -- a collection parameter may not narrow a prototype parameter typed iterable|int (the union is contravariant; narrowing is illegal)
--FILE--
<?php
interface P {
    public function p(iterable|int $x): void;
}
class C implements P {
    public function p(vec[int] $x): void {}
}
echo "unreachable\n";
?>
--EXPECTF--
Fatal error: Declaration of C::p(vec[int] $x): void must be compatible with P::p(Traversable|array|int $x): void in %s on line %d
