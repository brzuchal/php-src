--TEST--
set: union()/intersect()/diff() dispatch surfaces (dynamic name, case-insensitive, references)
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
$b = set[int]{3, 4, 5};

// dynamic method name
$m = 'union';     echo fmt($a->$m($b)), "\n";
$m = 'intersect'; echo fmt($a->$m($b)), "\n";
$m = 'diff';      echo fmt($a->$m($b)), "\n";

// case-insensitive dispatch
echo fmt($a->UNION($b)), "\n";
echo fmt($a->Intersect($b)), "\n";
$m = 'DiFF';      echo fmt($a->$m($b)), "\n";

// reference receiver
$r =& $a;
echo fmt($r->union($b)), "\n";

// chained references
$r1 =& $a; $r2 =& $r1;
echo fmt($r2->diff($b)), "\n";

// receiver never mutated
echo "a=", fmt($a), " count=", $a->count, "\n";
?>
--EXPECT--
[1, 2, 3, 4, 5]
[3]
[1, 2]
[1, 2, 3, 4, 5]
[3]
[1, 2]
[1, 2, 3, 4, 5]
[1, 2]
a=[1, 2, 3] count=3
