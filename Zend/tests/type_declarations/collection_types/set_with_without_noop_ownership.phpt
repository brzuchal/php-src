--TEST--
set: with()/without() no-op returns the receiver as an owned reference (reuse, no allocation)
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

// --- reuse: a no-op returns the SAME underlying set, not a copy ---
// zend_test_refcount() adds one temporary reference during its own call, so it
// reports (real refcount + 1). Before the no-op only $s references the set; after
// the no-op $a aliases it, so $s's refcount rises by exactly one. A copy would
// leave $s's refcount unchanged.
$s = set[int]{1, 2};
$before = zend_test_refcount($s);
$a = $s->with(2);                 // present -> no-op
$after = zend_test_refcount($s);
var_dump($after - $before === 1); // true: the no-op shares, it does not copy

$b = $s->without(99);             // absent -> no-op, shares again
var_dump(zend_test_refcount($s) - $before === 2);

// a changed result is a fresh, unshared set (refcount 1 -> reported as 2)
$c = $s->with(9);
var_dump(zend_test_refcount($c) === 2);

// --- no allocation on the no-op path ---
$s2 = set[int]{1, 2};
$m0 = memory_get_usage();
$x = $s2->with(2);                // no-op
$m1 = memory_get_usage();
$y = $s2->with(9);                // change
$m2 = memory_get_usage();
var_dump($m1 - $m0 === 0);        // no-op allocates nothing
var_dump($m2 - $m1 > 0);          // change allocates a new set

// --- explicit ownership: no-op results outlive the receiver ---
$r = set[int]{1, 2};
$keepAdd = $r->with(2);           // no-op
$keepRem = $r->without(99);       // no-op
unset($r);                        // the only other owner is gone

// $keepAdd and $keepRem must remain valid (balanced refcounting; ASAN would catch
// a use-after-free here if the no-op returned the receiver without an addref)
echo fmt($keepAdd), " ", fmt($keepRem), "\n";
var_dump($keepAdd->count, $keepRem->count);
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
[1, 2] [1, 2]
int(2)
int(2)
done
