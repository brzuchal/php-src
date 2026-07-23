--TEST--
contextual collection literals: head names remain ordinary class/type/name identifiers
--FILE--
<?php

/* vec/set/tuple/map/shape are heads only directly before '{' in expression
 * position. Everywhere a name or type is expected they stay ordinary
 * identifiers, including a body brace immediately after the name (class Vec{).
 * The scanner is case-inverted, so Set{ / MAP{ are the same head token and must
 * also stay usable as names. */

echo "-- declaration names, including an adjacent body brace --\n";
class Vec { const K = 'vec-class'; }
interface Map {}
trait Tuple { public function t(): string { return 'trait-tuple'; } }
enum Shape { case A; }
echo Vec::K, "\n";
echo (new class implements Map {}) instanceof Map ? "impl-map\n" : "FAIL\n";
echo (new class { use Tuple; })->t(), "\n";
echo Shape::A->name, "\n";

echo "-- extends / implements with head names (adjacent brace) --\n";
class Base {}
class DerivedFromHead extends Vec {}
echo (new DerivedFromHead) instanceof Vec ? "extends-ok\n" : "FAIL\n";

echo "-- head name as a parameter type, return type, property type --\n";
function takesVec(Vec $v): Vec { return $v; }
class Holder { public Vec $v; }
$h = new Holder();
$h->v = takesVec(new Vec());
echo $h->v::K, "\n";

echo "-- new / instanceof / :: / mixed casing --\n";
echo (new SET()) instanceof set ? "ci-instanceof\n" : "FAIL\n"; // class SET, used as 'set'
class SET {}
$x = new sEt();
echo $x instanceof SET ? "ci-new\n" : "FAIL\n";

echo "-- head names as function and constant names --\n";
function vec(int $x): int { return $x + 1; }
const set = 40;
echo vec(set + 1), "\n";

echo "-- first-class callable of a head-named function --\n";
$f = vec(...);
echo $f(7), "\n";

echo "-- trait use with an adjacent adaptation block --\n";
trait Alpha { public function a(): string { return 'A'; } }
class UsesHeadBrace {
    use Alpha {
        a as b;
    }
}
echo (new UsesHeadBrace)->b(), "\n";
?>
--EXPECT--
-- declaration names, including an adjacent body brace --
vec-class
impl-map
trait-tuple
A
-- extends / implements with head names (adjacent brace) --
extends-ok
-- head name as a parameter type, return type, property type --
vec-class
-- new / instanceof / :: / mixed casing --
ci-instanceof
ci-new
-- head names as function and constant names --
42
-- first-class callable of a head-named function --
8
-- trait use with an adjacent adaptation block --
A
