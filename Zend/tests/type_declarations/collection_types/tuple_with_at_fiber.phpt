--TEST--
tuple: withAt() FCC created outside, invoked inside a fiber
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$t = tuple[int, string]{1, "a"};
$f = $t->withAt(...);
unset($t);

$fiber = new Fiber(function () use ($f) {
    Fiber::suspend();
    echo fmt($f(0, 9)), "\n";   // [9, 'a']
    echo fmt($f(1, "b")), "\n"; // [1, 'b'] — original receiver reused
});
$fiber->start();
$fiber->resume();
echo "OK\n";
?>
--EXPECT--
[9, 'a']
[1, 'b']
OK
