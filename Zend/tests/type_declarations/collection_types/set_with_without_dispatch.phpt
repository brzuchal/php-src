--TEST--
set: with()/without() dispatch surfaces (dynamic name, case-insensitive, references)
--FILE--
<?php
function fmt(iterable $v): string {
    $out = [];
    foreach ($v as $x) {
        $out[] = var_export($x, true);
    }
    return "[" . implode(", ", $out) . "]";
}

$s = set[int]{1, 2, 3};

// dynamic method name
$m = 'with';    echo fmt($s->$m(4)), "\n";
$m = 'without'; echo fmt($s->$m(2)), "\n";

// case-insensitive dispatch
echo fmt($s->WITH(4)), "\n";
echo fmt($s->WithOut(1)), "\n";
$m = 'WITHOUT'; echo fmt($s->$m(3)), "\n";

// reference receiver
$r =& $s;
echo fmt($r->with(5)), "\n";
echo fmt($r->without(2)), "\n";

// chained references
$r1 =& $s; $r2 =& $r1;
echo fmt($r2->with(6)), "\n";

// receiver never mutated through any dispatch surface
echo "s=", fmt($s), " count=", $s->count, "\n";
?>
--EXPECT--
[1, 2, 3, 4]
[1, 3]
[1, 2, 3, 4]
[2, 3]
[1, 2]
[1, 2, 3, 5]
[1, 3]
[1, 2, 3, 6]
s=[1, 2, 3] count=3
