--TEST--
vec hybrid: 1000 simultaneously retained branches share one base and stay independent
--FILE--
<?php
/* Build a retained base, derive many live branches (each one retained append
 * -> hybrid sharing the base), then verify every branch's identity-carrying
 * tail element and the base's immutability; drop half the branches, collect,
 * and verify the survivors again. */
$base = vec[int]{};
for ($i = 0; $i < 500; $i++) {
    $base = $base->append($i);
}
var_dump($base->count);

$branches = [];
for ($b = 0; $b < 1000; $b++) {
    $branches[$b] = $base->append(10_000 + $b);
}

$ok = true;
foreach ($branches as $b => $v) {
    if ($v->count !== 501 || $v[500] !== 10_000 + $b || $v[0] !== 0 || $v[499] !== 499) {
        $ok = false;
        echo "branch $b corrupted\n";
        break;
    }
}
var_dump($ok, $base->count, $base[499]);

/* Drop every even branch, force a collection cycle, re-verify the odd ones. */
for ($b = 0; $b < 1000; $b += 2) {
    unset($branches[$b]);
}
gc_collect_cycles();

$ok = true;
$sum = 0;
foreach ($branches as $b => $v) {
    if ($v->count !== 501 || $v[500] !== 10_000 + $b) {
        $ok = false;
        break;
    }
    $sum += $v[500] - 10_000;
}
var_dump($ok, $sum === array_sum(array_keys($branches)));

/* Branches must not alias each other's logical value. */
var_dump($branches[1] === $branches[3]);

unset($branches);
gc_collect_cycles();
var_dump($base->count, $base[0], $base[499]);
?>
--EXPECT--
int(500)
bool(true)
int(500)
int(499)
bool(true)
bool(true)
bool(false)
int(500)
int(0)
int(499)
