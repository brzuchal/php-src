--TEST--
set: with()/without() FCCs created outside, invoked inside a fiber
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$s = set[int]{1, 2};
$w = $s->with(...);
$wo = $s->without(...);
unset($s);

$fiber = new Fiber(function () use ($w, $wo) {
    Fiber::suspend();
    echo fmt($w(3)), "\n";   // [1, 2, 3]
    echo fmt($wo(1)), "\n";  // [2]
    echo fmt($w(2)), "\n";   // [1, 2] — no-op, original receiver
});
$fiber->start();
$fiber->resume();
echo "OK\n";
?>
--EXPECT--
[1, 2, 3]
[2]
[1, 2]
OK
