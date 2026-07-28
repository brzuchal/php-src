--TEST--
F2 OQ-1: richer-union variance -- a collection (and iterable) may narrow a prototype return typed iterable|int or iterable|Foo, and may widen a collection parameter to such a union
--FILE--
<?php
class Foo {}

interface P {
    public function r1(): iterable|int;   // covariant return: children may narrow
    public function r2(): iterable|Foo;
    public function p1(vec[int] $x): void; // contravariant param: children may widen
}

class C implements P {
    public function r1(): iterable { return vec[int]{1}; }   // union -> iterable
    public function r2(): vec[int] { return vec[int]{2}; }   // union -> collection
    public function p1(iterable|int $x): void {}             // collection -> union
}

class D implements P {
    public function r1(): vec[int] { return vec[int]{3}; }   // union -> collection
    public function r2(): iterable { return vec[int]{4}; }   // union -> iterable
    public function p1(iterable $x): void {}                 // collection -> iterable
}

$c = new C;
$d = new D;

echo get_debug_type($c->r1()), "\n";   // collection
echo get_debug_type($c->r2()), "\n";   // collection
$c->p1(vec[int]{1}); echo "C::p1 ok\n";
echo get_debug_type($d->r1()), "\n";   // collection
echo get_debug_type($d->r2()), "\n";   // collection
$d->p1(vec[int]{9}); echo "D::p1 ok\n";
echo "ok\n";
?>
--EXPECT--
collection
collection
C::p1 ok
collection
collection
D::p1 ok
ok
