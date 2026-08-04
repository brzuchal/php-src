--TEST--
Collection foreach + F2 iterability: foreach works, is_iterable() is true, iterable params accept
--DESCRIPTION--
F1 shipped foreach only, with is_iterable() false as an interim state. F2 makes
collections fully iterable: is_iterable() is true and the iterable type accepts
them. A plain object remains foreach-able over its properties yet is not
is_iterable(), which is unchanged.
--FILE--
<?php
$c = vec[int]{1, 2, 3};

$sum = 0;
foreach ($c as $v) { $sum += $v; }
echo "foreach sum: $sum\n";

var_dump(is_iterable($c));
var_dump(is_iterable(new stdClass)); // a plain object is not is_iterable() (unchanged)

function consume(iterable $x): string { return "accepted"; }
echo consume($c), "\n";
?>
--EXPECT--
foreach sum: 6
bool(true)
bool(false)
accepted
