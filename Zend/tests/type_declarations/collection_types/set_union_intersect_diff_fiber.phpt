--TEST--
set: union()/intersect()/diff() FCCs created outside, invoked inside a fiber
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
$u = $a->union(...);
$i = $a->intersect(...);
$d = $a->diff(...);
unset($a);

$fiber = new Fiber(function () use ($u, $i, $d) {
    Fiber::suspend();
    echo fmt($u(set[int]{3, 4})), "\n";   // [1, 2, 3, 4]
    echo fmt($i(set[int]{2, 3})), "\n";   // [2, 3]
    echo fmt($d(set[int]{1})), "\n";      // [2, 3]
});
$fiber->start();
$fiber->resume();
echo "OK\n";
?>
--EXPECT--
[1, 2, 3, 4]
[2, 3]
[2, 3]
OK
