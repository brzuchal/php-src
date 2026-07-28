--TEST--
F2: opcache persistence preserves the iterable provenance on richer unions (iterable|int, iterable|Foo) -- collections accepted, explicit unions still reject, Reflection unchanged
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) die('skip Zend OPcache required');
?>
--INI--
opcache.enable_cli=1
opcache.jit=disable
--FILE--
<?php
class Foo {}
function f(iterable|int $x): string { return is_int($x) ? "int" : get_debug_type($x); }
function g(iterable|Foo $x): string { return get_debug_type($x); }
function h(array|Traversable|int $x): string { return is_int($x) ? "int" : get_debug_type($x); }

echo f(vec[int]{1, 2}), "\n";  // provenance survives persist -> collection accepted
echo f(5), "\n";               // int member still accepted
echo g(set[int]{1}), "\n";     // collection accepted
try { h(vec[int]{1}); } catch (\TypeError $e) { echo "explicit union rejects\n"; }
echo h(9), "\n";               // explicit union still accepts its own members

$t = (new ReflectionFunction('f'))->getParameters()[0]->getType();
echo get_class($t), " ", (string) $t, "\n";
echo "ok\n";
?>
--EXPECT--
collection
int
collection
explicit union rejects
int
ReflectionUnionType Traversable|array|int
ok
