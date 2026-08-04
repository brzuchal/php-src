--TEST--
F2: opcache persistence preserves the iterable provenance on iterable|null (accepts collections; Reflection unchanged)
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) die('skip Zend OPcache required');
?>
--INI--
opcache.enable_cli=1
opcache.jit=disable
--FILE--
<?php
function f(iterable|null $x): string { return $x === null ? "null" : get_debug_type($x); }
function g(array|Traversable|null $x): string { return $x === null ? "null" : get_debug_type($x); }

echo f(vec[int]{1, 2}), "\n";
echo f(null), "\n";
try { g(vec[int]{1}); } catch (\TypeError $e) { echo "explicit union rejects\n"; }

$t = (new ReflectionFunction('f'))->getParameters()[0]->getType();
echo get_class($t), " ", (string) $t, "\n";
echo "ok\n";
?>
--EXPECT--
collection
null
explicit union rejects
ReflectionUnionType Traversable|array|null
ok
