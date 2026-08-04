--TEST--
Bare collection-kind types under opcache (SHM persist path)
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) die('skip Zend OPcache required');
?>
--INI--
opcache.enable_cli=1
opcache.jit=disable
--FILE--
<?php
function f(vec[] $v): vec[] { return $v; }
$r = f(vec[int]{1, 2, 3});
echo $r->count, "\n";
try { f(set[int]{1}); } catch (\TypeError $e) { echo "rejected\n"; }
var_dump((new ReflectionFunction('f'))->getReturnType()->getCollectionName());
echo (string) (new ReflectionFunction('f'))->getParameters()[0]->getType(), "\n";
echo "ok\n";
?>
--EXPECT--
3
rejected
string(3) "vec"
vec[]
ok
