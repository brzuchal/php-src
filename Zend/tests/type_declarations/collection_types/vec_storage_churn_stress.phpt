--TEST--
vec storage: repeated create/update/destroy churn across representations and growth thresholds
--FILE--
<?php
/* Exercise every storage path in a tight loop: exclusive consume chains
 * crossing the capacity growth steps, retained appends producing hybrids,
 * flatten crossings for both policy triggers (absolute tail bound on a large
 * base, tail/base ratio on a small base), and the flatten-fallback update ops
 * on hybrid receivers -- with periodic cycle collection. Elements are checked
 * by closed form, so any storage bug surfaces as a wrong value. */

function checksum($v): int {
    $s = $v->count;
    foreach ($v as $e) {
        $s += $e;
    }
    return $s;
}

$rounds = 200;
$ok = true;
for ($r = 0; $r < $rounds; $r++) {
    /* exclusive TMP chain across small growth steps (capacity 4 -> 12 -> ...) */
    $n = 3 + ($r % 29);
    $x = vec[int]{};
    for ($i = 0; $i < $n; $i++) {
        $x = $x->append($i);
    }
    if (checksum($x) !== $n + intdiv($n * ($n - 1), 2)) {
        $ok = false;
        echo "chain round $r broken\n";
        break;
    }

    /* retained append (hybrid) + interleaved update-op fallbacks */
    $keep = $x;
    $h = $keep->append($n);
    $w = $h->withAt(0, 7);
    $p = $h->prepend(-1);
    $d = $h->withoutAt(0);
    if ($h->count !== $n + 1 || $w[0] !== 7 || $p[0] !== -1 || $d->count !== $n
        || $keep->count !== $n || ($n > 0 && $keep[0] !== 0)) {
        $ok = false;
        echo "update round $r broken\n";
        break;
    }

    /* ratio trigger: tail outgrows a small base -> flattens, value intact */
    $small = vec[int]{1, 2};
    $g = $small->append(3)->append(4)->append(5)->append(6);
    if (checksum($g) !== 6 + 21 || $small->count !== 2) {
        $ok = false;
        echo "ratio round $r broken\n";
        break;
    }

    if ($r % 50 === 0) {
        gc_collect_cycles();
    }
    unset($x, $keep, $h, $w, $p, $d, $small, $g);
}
var_dump($ok);

/* absolute tail bound: push a retained value far past T on a large base; the
 * logical value must be exact regardless of how often it flattened. */
$base = vec[int]{};
for ($i = 0; $i < 400; $i++) {
    $base = $base->append($i);
}
$acc = $base;
for ($i = 400; $i < 800; $i++) {
    $keepAlive = $acc;          /* retain each step: forces the shared path */
    $acc = $acc->append($i);
}
var_dump($acc->count, $acc[0], $acc[399], $acc[400], $acc[799], $keepAlive->count);
var_dump(checksum($acc) === 800 + intdiv(800 * 799, 2));

gc_collect_cycles();
var_dump($base->count);
?>
--EXPECT--
bool(true)
int(800)
int(0)
int(399)
int(400)
int(799)
int(799)
bool(true)
int(400)
