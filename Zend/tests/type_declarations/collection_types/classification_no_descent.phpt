--TEST--
Collection types: cached metadata answers checks without descending
--EXTENSIONS--
zend_test
--SKIPIF--
<?php
if (zend_test_collection_descents() === null) {
    die("skip requires a debug build (descent counter)");
}
?>
--FILE--
<?php

/* "Descending" means a member comparison that had to walk into a nested node.
 * Cached classification is what avoids it: a node whose members are all builtin
 * masks is compared without inspecting members as structured types at all.
 *
 * The counter is cumulative, so every measurement below is taken in steady
 * state -- after warm-up, when every node already exists and each lookup is a
 * chain hit. A first lookup is a miss and recurses into child *promotion*
 * instead, which is a different (one-off) cost. */

function takes_int(vec[int] $x): void {}
function takes_n1(vec[vec[int]] $x): void {}
function takes_n2(vec[vec[vec[int]]] $x): void {}

zend_test_make_vec([1, 2, 3], 'int', $flat);
zend_test_make_vec([$flat], 'vec:int', $nested);
foreach (['takes_int', 'takes_n1', 'takes_n2'] as $fn) {
    zend_test_collection_intern($fn);
}

$d = fn() => zend_test_collection_descents();

echo "-- flat types never descend, however often they are used --\n";
$a = $d();
for ($i = 0; $i < 50; $i++) {
    takes_int($flat);                          /* type check   */
    zend_test_make_vec([1, 2], 'int', $tmp);   /* construction */
    zend_test_collection_intern('takes_int');  /* promotion    */
}
var_dump($d() - $a === 0);

echo "-- reading a value's canonical node never descends --\n";
$a = $d();
for ($i = 0; $i < 50; $i++) {
    zend_test_vec_type_id($nested);
}
var_dump($d() - $a === 0);

echo "-- element checks against a nested type are pointer comparisons --\n";
/* Appending a vec[int] into a vec[vec[int]] compares canonical nodes, so the
 * element check itself contributes nothing beyond the single lookup. */
$a = $d();
zend_test_make_vec([$flat, $flat, $flat], 'vec:int', $tmp);
var_dump($d() - $a === 1);

echo "-- what still descends: the descriptor side of a lookup --\n";
/* The value side is canonical, but a declaration is still a compiler
 * descriptor, so confirming it costs one descent per nesting level. This is the
 * PROBE path and it is required for correctness: a chain hit must be confirmed
 * structurally, or two types sharing a key would alias. Caching the promoted
 * declaration in the VM removes it, and that is the next stage. */
$a = $d();
takes_n1($nested);
var_dump($d() - $a === 1);

$a = $d();
zend_test_collection_intern('takes_n2');
var_dump($d() - $a === 2);

echo "-- and only on the first check: the resolution cache absorbs the rest --\n";
/* takes_n1 was resolved just above, so every further check answers from the
 * resolution cache and descends nothing at all. */
$a = $d();
for ($i = 0; $i < 10; $i++) {
    takes_n1($nested);
}
var_dump($d() - $a === 0);

?>
--EXPECT--
-- flat types never descend, however often they are used --
bool(true)
-- reading a value's canonical node never descends --
bool(true)
-- element checks against a nested type are pointer comparisons --
bool(true)
-- what still descends: the descriptor side of a lookup --
bool(true)
bool(true)
-- and only on the first check: the resolution cache absorbs the rest --
bool(true)
