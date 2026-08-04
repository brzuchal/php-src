--TEST--
tuple: withAt() dispatch surfaces (dynamic name, case-insensitive, references)
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

// dynamic method name
$m = 'withAt'; echo fmt($t->$m(0, 9)), "\n";

// case-insensitive dispatch
echo fmt($t->WITHAT(1, "B")), "\n";
echo fmt($t->WithAt(0, 7)), "\n";
$m = 'WITHAT'; echo fmt($t->$m(1, "c")), "\n";

// reference receiver
$r =& $t;
echo fmt($r->withAt(0, 5)), "\n";

// chained references
$r1 =& $t; $r2 =& $r1;
echo fmt($r2->withAt(1, "z")), "\n";

// receiver never mutated through any dispatch surface
echo "t=", fmt($t), " count=", $t->count, "\n";
?>
--EXPECT--
[9, 'a']
[1, 'B']
[7, 'a']
[1, 'c']
[5, 'a']
[1, 'z']
t=[1, 'a'] count=2
