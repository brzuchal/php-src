--TEST--
Collection intrinsic properties under tracing JIT: correct values, no crash
--DESCRIPTION--
Tracing JIT side-exits to the VM for a collection property fetch (the recorded
IS_OBJECT guard fails), so the VM intrinsic-property branch produces the value.
This exercises a hot loop reading intrinsic properties and checks correctness.
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
$v = vec[int]{1, 2, 3};
$e = vec[int]{};
$sum = 0;
for ($i = 0; $i < 100000; $i++) {
    $sum += $v->count;
    if ($e->isEmpty) { $sum += 1; }
}
var_dump($sum);   // 100000*3 + 100000 = 400000
var_dump($v->count, $v->isEmpty, $e->count, $e->isEmpty);
echo "ok\n";
?>
--EXPECT--
int(400000)
int(3)
bool(false)
int(0)
bool(true)
ok
