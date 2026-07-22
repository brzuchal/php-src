--TEST--
Collection types: declarations resolve to canonical nodes exactly once
--EXTENSIONS--
zend_test
--SKIPIF--
<?php
if (zend_test_collection_stats() === null) {
    die("skip requires a debug build (cache counters)");
}
?>
--FILE--
<?php

/* A type check starts from a *declaration* -- a compiler descriptor. Resolving
 * it walks the descriptor and promotes it to a canonical node. The resolution
 * cache makes that happen at most once per descriptor per request; afterwards
 * the check is a pointer comparison between two canonical nodes. */

function takes_int(vec[int] $x): void {}
function takes_nested(vec[vec[int]] $x): void {}
function takes_deep(vec[vec[vec[int]]] $x): void {}

class Holder {
    public vec[vec[int]] $prop;
}

zend_test_make_vec([1, 2, 3], 'int', $flat);
zend_test_make_vec([$flat], 'vec:int', $nested);

$s = fn() => zend_test_collection_stats();

echo "-- first execution promotes --\n";
$a = $s();
takes_int($flat);
$b = $s();
var_dump($b['promotions'] - $a['promotions'] === 1);

echo "-- subsequent executions reuse the cached node --\n";
$a = $s();
for ($i = 0; $i < 100; $i++) {
    takes_int($flat);
}
$b = $s();
var_dump($b['promotions'] - $a['promotions'] === 0);

echo "-- nested declarations descend once, then never again --\n";
$a = $s();
takes_nested($nested);      /* first: resolves, walks the descriptor */
$b = $s();
var_dump($b['promotions'] - $a['promotions'] === 1);

$a = $s();
for ($i = 0; $i < 100; $i++) {
    takes_nested($nested);
}
$b = $s();
/* The whole point of this stage: after warm-up a nested declaration check
 * performs no promotion and no descriptor descent at all. */
var_dump($b['promotions'] - $a['promotions'] === 0);
var_dump($b['descents'] - $a['descents'] === 0);

echo "-- repeated executions allocate no additional nodes --\n";
$a = $s();
for ($i = 0; $i < 100; $i++) {
    takes_int($flat);
    takes_nested($nested);
}
$b = $s();
var_dump($b['nodes'] - $a['nodes'] === 0);

echo "-- property declarations resolve through the same cache --\n";
$h = new Holder();
$a = $s();
$h->prop = $nested;
$b = $s();
$first = $b['promotions'] - $a['promotions'];
$a = $s();
for ($i = 0; $i < 50; $i++) {
    $h->prop = $nested;
}
$b = $s();
var_dump($first === 1, $b['promotions'] - $a['promotions'] === 0);

echo "-- a distinct declaration is resolved on its own first use --\n";
/* Resolution is a property of the declaration, not of the value: a failing
 * check still resolves vec[vec[vec[int]]] exactly once, and not again. */
$a = $s();
try { takes_deep($flat); } catch (TypeError $e) {}
$b = $s();
var_dump($b['promotions'] - $a['promotions'] === 1);

$a = $s();
for ($i = 0; $i < 20; $i++) {
    try { takes_deep($flat); } catch (TypeError $e) {}
}
$b = $s();
var_dump($b['promotions'] - $a['promotions'] === 0);

echo "-- mismatches stay rejected and do not promote again --\n";
$a = $s();
for ($i = 0; $i < 20; $i++) {
    try {
        takes_int($nested);
    } catch (TypeError $e) {
    }
}
$b = $s();
var_dump($b['promotions'] - $a['promotions'] === 0);
var_dump($b['nodes'] - $a['nodes'] === 0);

?>
--EXPECT--
-- first execution promotes --
bool(true)
-- subsequent executions reuse the cached node --
bool(true)
-- nested declarations descend once, then never again --
bool(true)
bool(true)
bool(true)
-- repeated executions allocate no additional nodes --
bool(true)
-- property declarations resolve through the same cache --
bool(true)
bool(true)
-- a distinct declaration is resolved on its own first use --
bool(true)
bool(true)
-- mismatches stay rejected and do not promote again --
bool(true)
bool(true)
