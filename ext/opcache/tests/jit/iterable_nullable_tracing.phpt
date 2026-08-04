--TEST--
F2: nullable iterable (?iterable / iterable|null) carrying collections under tracing JIT
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
function s(iterable|null $x): int { if ($x === null) return 0; $n = 0; foreach ($x as $v) { $n += $v; } return $n; }
$v = vec[int]{1, 2, 3};
$acc = 0;
for ($i = 0; $i < 100000; $i++) { $acc += s($v); $acc += s(null); }
var_dump($acc);          // 100000 * 6
var_dump(is_iterable($v));
echo "ok\n";
?>
--EXPECT--
int(600000)
bool(true)
ok
