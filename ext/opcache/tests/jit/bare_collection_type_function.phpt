--TEST--
Bare collection-kind types (vec[]) under function JIT
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) die('skip Zend OPcache required');
?>
--INI--
opcache.enable_cli=1
opcache.jit_buffer_size=64M
opcache.jit=1205
opcache.jit_hot_func=2
--FILE--
<?php
function f(vec[] $v): vec[] { return $v; }
function pv($c): int { return $c->count; }   // untyped param -> JIT-compiled

$v = vec[int]{1, 2, 3};
for ($i = 0; $i < 100000; $i++) { f($v); pv($v); }

var_dump(f($v)->count);                 // 3
var_dump(pv($v));                       // 3
var_dump(f($v) instanceof \Countable);  // false (unchanged)
try { f(set[int]{1}); } catch (\TypeError $e) { echo "rejected\n"; }
echo "ok\n";
?>
--EXPECT--
int(3)
int(3)
bool(false)
rejected
ok
