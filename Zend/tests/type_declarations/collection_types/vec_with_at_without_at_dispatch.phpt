--TEST--
vec: withAt()/withoutAt() dispatch surfaces (dynamic name, case-insensitive, references)
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

// dynamic method name
$m = 'withAt';    echo fmt($v->$m(1, 99)), "\n";
$m = 'withoutAt'; echo fmt($v->$m(1)), "\n";

// case-insensitive dispatch
echo fmt($v->WITHAT(0, 7)), "\n";
echo fmt($v->WithoutAt(2)), "\n";
$m = 'WITHOUTAT'; echo fmt($v->$m(0)), "\n";

// reference receiver
$r =& $v;
echo fmt($r->withAt(1, 5)), "\n";
echo fmt($r->withoutAt(0)), "\n";

// chained references
$r1 =& $v; $r2 =& $r1;
echo fmt($r2->withAt(2, 9)), "\n";

// receiver never mutated through any dispatch surface
echo "v=", fmt($v), " count=", $v->count, "\n";
?>
--EXPECT--
[10, 99, 30]
[10, 30]
[7, 20, 30]
[10, 20]
[20, 30]
[10, 5, 30]
[20, 30]
[10, 20, 9]
v=[10, 20, 30] count=3
