--TEST--
Collection foreach: set iterates first-occurrence order with position keys after === dedup
--FILE--
<?php
$s = set[int]{3, 1, 3, 2, 1};
$pairs = [];
foreach ($s as $k => $v) { $pairs[] = "$k:$v"; }
echo implode(" ", $pairs), "\n";

$s2 = set[string]{"b", "a", "b", "c"};
$pairs = [];
foreach ($s2 as $k => $v) { $pairs[] = "$k:$v"; }
echo implode(" ", $pairs), "\n";
?>
--EXPECT--
0:3 1:1 2:2
0:b 1:a 2:c
