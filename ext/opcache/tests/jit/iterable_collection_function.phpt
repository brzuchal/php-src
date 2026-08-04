--TEST--
F2: iterable-typed native collections under function JIT
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
function sum(iterable $x): int { $n = 0; foreach ($x as $v) { $n += $v; } return $n; }
function cnt(iterable $x): int { return iterator_count($x); }

$v = vec[int]{1, 2, 3};
$s = 0;
for ($i = 0; $i < 100000; $i++) { $s += sum($v); $s += cnt($v); }
var_dump($s);                                  // 100000 * (6 + 3)
var_dump(is_iterable($v));
var_dump(sum(set[int]{4, 5}) + cnt(set[int]{4, 5}));   // 9 + 2
try { sum(new stdClass); } catch (\TypeError $e) { echo "rejected\n"; }
echo "ok\n";
?>
--EXPECT--
int(900000)
bool(true)
int(11)
rejected
ok
