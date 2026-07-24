--TEST--
Collection foreach: nested and concurrent loops over one value keep independent positions
--FILE--
<?php
$c = vec[int]{1, 2, 3};
$out = [];
foreach ($c as $a) {
    foreach ($c as $b) {
        $out[] = "$a$b";
    }
}
echo implode(" ", $out), "\n";

$sum = 0;
foreach ($c as $x) { $sum += $x; }
foreach ($c as $x) { $sum += $x; }
echo "sum: $sum\n";
?>
--EXPECT--
11 12 13 21 22 23 31 32 33
sum: 12
