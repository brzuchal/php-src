--TEST--
F2: nullable iterable (?iterable / iterable|null) carrying collections under function JIT
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
function s(?iterable $x): int { if ($x === null) return 0; $n = 0; foreach ($x as $v) { $n += $v; } return $n; }
function u(iterable|null $x): int { return $x === null ? -1 : iterator_count($x); }

$v = vec[int]{1, 2, 3};
$acc = 0;
for ($i = 0; $i < 100000; $i++) { $acc += s($v); $acc += s(null); $acc += u($v); }
var_dump($acc);          // 100000 * (6 + 0 + 3)
var_dump(s(null), u(null));
try { s(new stdClass); } catch (\TypeError $e) { echo "rejected\n"; }
echo "ok\n";
?>
--EXPECT--
int(900000)
int(0)
int(-1)
rejected
ok
