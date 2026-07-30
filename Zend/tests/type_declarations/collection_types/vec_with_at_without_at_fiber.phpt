--TEST--
vec: withAt()/withoutAt() FCCs created outside, invoked inside a fiber
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$v = vec[int]{10, 20, 30};
$wa = $v->withAt(...);
$wo = $v->withoutAt(...);
unset($v);

$fiber = new Fiber(function () use ($wa, $wo) {
    Fiber::suspend();
    echo fmt($wa(1, 99)), "\n";   // [10, 99, 30]
    echo fmt($wo(0)), "\n";       // [20, 30]
});
$fiber->start();
$fiber->resume();
echo "OK\n";
?>
--EXPECT--
[10, 99, 30]
[20, 30]
OK
