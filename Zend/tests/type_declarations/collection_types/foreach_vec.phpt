--TEST--
Collection foreach: vec yields values with integer keys 0..n-1 in storage order
--FILE--
<?php
$v = vec[int]{10, 20, 30};

$vals = [];
foreach ($v as $x) { $vals[] = $x; }
echo implode(",", $vals), "\n";

$pairs = [];
foreach ($v as $k => $x) { $pairs[] = "$k=>$x"; }
echo implode(" ", $pairs), "\n";

$s = vec[string]{"c", "a", "b"};
$pairs = [];
foreach ($s as $k => $x) { $pairs[] = "$k:$x"; }
echo implode(" ", $pairs), "\n";
?>
--EXPECT--
10,20,30
0=>10 1=>20 2=>30
0:c 1:a 2:b
