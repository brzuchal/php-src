--TEST--
Bare collection-kind types (vec[]) under tracing JIT
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) die('skip Zend OPcache required');
?>
--INI--
opcache.enable_cli=1
opcache.jit_buffer_size=64M
opcache.jit=tracing
opcache.jit_hot_loop=1
opcache.jit_hot_func=2
--FILE--
<?php
function f(vec[] $v): vec[] { return $v; }
$v = vec[int]{1, 2, 3};
$sum = 0;
for ($i = 0; $i < 100000; $i++) { $sum += f($v)->count; }
var_dump($sum);                    // 300000
try { f(set[int]{1}); } catch (\TypeError $e) { echo "rejected\n"; }
echo "ok\n";
?>
--EXPECT--
int(300000)
rejected
ok
