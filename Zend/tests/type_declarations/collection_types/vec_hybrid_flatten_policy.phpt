--TEST--
vec hybrid: flatten policy — new-value only, never mutates a shared value; update fallbacks correct
--EXTENSIONS--
zend_test
--FILE--
<?php
function arr(mixed $v): array { return iterator_to_array($v); }

zend_test_make_vec(range(0, 99), 'int', $b);
$h = $b->append(100);                 // hybrid [0..99 | 100]
$hv = [...range(0, 99), 100];         // its logical value 0..100

// (1) A read-only operation NEVER flattens or replaces a shared hybrid: after a
// storm of reads/iterations/serializes/identity, memory is unchanged.
$m0 = memory_get_usage();
for ($i = 0; $i < 500; $i++) { $h[50]; iterator_to_array($h); serialize($h); $h === $b; }
// A flatten would permanently add ~base*16 bytes; no-flatten leaves it ~0.
printf("read-only leaves shared hybrid intact: %s\n", (memory_get_usage() - $m0) < 256 ? 'yes' : 'NO');

// (2) Update-op fallbacks on a hybrid produce the correct value and leave the
// receiver unchanged (flatten is a new value, never an in-place mutation).
printf("prepend: %s\n",    arr($h->prepend(-1)) === [-1, ...$hv] ? 'ok' : 'NO');
printf("withAt: %s\n",     arr($h->withAt(0, 999)) === [999, ...array_slice($hv, 1)] ? 'ok' : 'NO');
printf("withoutAt: %s\n",  arr($h->withoutAt(100)) === array_slice($hv, 0, 100) ? 'ok' : 'NO');
printf("receiver unchanged: %s\n", arr($h) === $hv ? 'yes' : 'NO');

// (3) The absolute bound T flattens a long exclusive chain into a correct flat value.
zend_test_make_vec([0], 'int', $one);
$chain = $one;
for ($i = 1; $i <= 300; $i++) $chain = $chain->append($i);   // crosses T -> flat
printf("T-bound chain correct: %s\n", arr($chain) === range(0, 300) ? 'yes' : 'NO');

// (4) The ratio bound flattens a small-base chain (tail may not outgrow R*base).
zend_test_make_vec([0, 1, 2, 3], 'int', $sb);
$sc = $sb->append(4)->append(5)->append(6)->append(7)->append(8);  // tail passes base -> flatten
printf("ratio chain correct: %s\n", arr($sc) === range(0, 8) ? 'yes' : 'NO');

// (5) R degenerate case: appending to a SHARED empty vec publishes a FLAT
// value, not a hybrid over an empty base (which would put tail > R*base).
// The representation itself is asserted by the C-level policy selftest
// (zend_test_vec_selftest: policy_empty_base_flat); here we prove the
// semantics: correct result, untouched receiver, chains keep working.
zend_test_make_vec([], 'int', $e0);
$r0 = $e0->append(1);
printf("empty-base append: %s\n", arr($r0) === [1] && arr($e0) === [] ? 'ok' : 'NO');
printf("empty-base chain: %s\n", arr($e0->append(1)->append(2)) === [1, 2] ? 'ok' : 'NO');
?>
--EXPECT--
read-only leaves shared hybrid intact: yes
prepend: ok
withAt: ok
withoutAt: ok
receiver unchanged: yes
T-bound chain correct: yes
ratio chain correct: yes
empty-base append: ok
empty-base chain: ok
