--TEST--
Collection foreach under function JIT: cached $main cleanup must not spin on ht-iterator
--DESCRIPTION--
Regression for the shared JIT FE_FREE ht-iterator-delete seam. A collection keeps
its foreach position in u2.fe_pos, which aliases fe_iter_idx, and owns no
EG(ht_iterators) slot, so the JIT-compiled FE_FREE (zend_jit_free) must skip
zend_hash_iterator_del() for a collection -- exactly as the VM handler does. A
single collection foreach in an opcache-cached $main under function JIT (jit=1205)
infinite-looped in zend_hash_remove_iterator_copies. Bounded by max_execution_time
so a regression fails fast instead of stalling CI. Not nesting-specific: the first
case is the now-minimal single loop in $main.
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) die('skip Zend OPcache required');
?>
--INI--
opcache.enable_cli=1
opcache.jit_buffer_size=64M
opcache.jit=1205
opcache.jit_hot_func=1
opcache.jit_hot_loop=1
max_execution_time=5
--FILE--
<?php
// minimal reproducer: a single collection foreach at top level ($main)
$c = vec[int]{1, 2, 3};
foreach ($c as $v) {}
echo "single ok\n";

// original reproducer: nested + sequential collection loops in $main
$o = [];
foreach ($c as $a) { foreach ($c as $b) { $o[] = "$a$b"; } }
echo implode(" ", $o), "\n";
$s = 0;
foreach ($c as $x) { $s += $x; }
foreach ($c as $x) { $s += $x; }
echo "sum: $s\n";

// values and keys remain correct under JIT (set: first-occurrence order, position keys)
$out = [];
foreach (set[int]{3, 1, 3, 2} as $k => $val) { $out[] = "$k:$val"; }
echo implode(" ", $out), "\n";
?>
--EXPECT--
single ok
11 12 13 21 22 23 31 32 33
sum: 12
0:3 1:1 2:2
