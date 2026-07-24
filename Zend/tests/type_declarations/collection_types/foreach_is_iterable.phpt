--TEST--
Collection foreach (F1/Package 1): foreach works but is_iterable() is false and iterable params reject
--DESCRIPTION--
F1 ships foreach only. is_iterable() stays false and the iterable type keeps
rejecting collections -- the same asymmetry a plain object already has. This is
an interim milestone state; full iterability is the mandatory F2 milestone.
--FILE--
<?php
$c = vec[int]{1, 2, 3};

$sum = 0;
foreach ($c as $v) { $sum += $v; }
echo "foreach sum: $sum\n";

var_dump(is_iterable($c));
var_dump(is_iterable(new stdClass)); // a plain object is also foreach-able yet not is_iterable()

function consume(iterable $x): string { return "accepted"; }
try {
    echo consume($c), "\n";
} catch (\TypeError $e) {
    echo "TypeError: iterable param rejects a collection in F1\n";
}
?>
--EXPECT--
foreach sum: 6
bool(false)
bool(false)
TypeError: iterable param rejects a collection in F1
