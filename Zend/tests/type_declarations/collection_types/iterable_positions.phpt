--TEST--
F2: collections satisfy the iterable type in param, return, property and reference positions
--FILE--
<?php
function p(iterable $v): string { return get_debug_type($v); }
echo p(vec[int]{1, 2}), "\n";
echo p(set[int]{1}), "\n";
echo p(tuple[int]{1}), "\n";

function r($v): iterable { return $v; }
var_dump(is_iterable(r(vec[int]{1})));

class Box { public iterable $p; }
$b = new Box();
$b->p = vec[int]{1, 2, 3};
echo get_debug_type($b->p), "\n";

$ref = vec[int]{9};
function byref(iterable &$x): void { $x = $x; }
byref($ref);
echo get_debug_type($ref), "\n";
echo "ok\n";
?>
--EXPECT--
collection
collection
collection
bool(true)
collection
collection
ok
