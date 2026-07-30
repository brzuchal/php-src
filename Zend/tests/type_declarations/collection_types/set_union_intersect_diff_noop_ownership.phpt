--TEST--
set: union()/intersect()/diff() empty-effect results reuse the receiver (no allocation)
--EXTENSIONS--
zend_test
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$a = set[int]{1, 2, 3};
$sub = set[int]{1, 2};          // ⊆ a  -> union no-op
$sup = set[int]{1, 2, 3, 4};    // ⊇ a  -> intersect no-op
$dis = set[int]{7, 8};          // disjoint -> diff no-op

// reuse: a no-op raises the receiver's refcount by exactly one (a copy would not).
// zend_test_refcount adds one temporary ref during its call, so we compare deltas.
$r = zend_test_refcount($a); $u = $a->union($sub);
var_dump(zend_test_refcount($a) - $r === 1);
$r = zend_test_refcount($a); $i = $a->intersect($sup);
var_dump(zend_test_refcount($a) - $r === 1);
$r = zend_test_refcount($a); $d = $a->diff($dis);
var_dump(zend_test_refcount($a) - $r === 1);

// a changed result is a fresh, unshared set (refcount 1 -> reported as 2)
$c = $a->union(set[int]{9});
var_dump(zend_test_refcount($c) === 2);

// zero allocation on the no-op path; a change allocates
gc_collect_cycles();
$m0 = memory_get_usage(); $x = $a->intersect($sup); $m1 = memory_get_usage();
var_dump($m1 - $m0 === 0);
$m0 = memory_get_usage(); $y = $a->union(set[int]{9}); $m1 = memory_get_usage();
var_dump($m1 - $m0 > 0);

// explicit ownership: no-op results outlive the receiver (ASAN would catch a UAF)
$keepU = $a->union($sub);
$keepI = $a->intersect($sup);
$keepD = $a->diff($dis);
unset($a, $sub, $sup, $dis);
echo fmt($keepU), " ", fmt($keepI), " ", fmt($keepD), "\n";
var_dump($keepU->count, $keepI->count, $keepD->count);
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
[1, 2, 3] [1, 2, 3] [1, 2, 3]
int(3)
int(3)
int(3)
done
