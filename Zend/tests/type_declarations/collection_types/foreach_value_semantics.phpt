--TEST--
Collection foreach: by-value copies -- COW arrays, shared object identity, nested immutability
--FILE--
<?php
// contained array is copy-on-write: mutating the yielded copy leaves the element intact
$va = vec[array]{[1, 2]};
foreach ($va as $arr) { $arr[] = 99; }
$counts = [];
foreach ($va as $arr) { $counts[] = count($arr); }
echo "array counts after mutation: ", implode(",", $counts), "\n";

// contained object keeps shared identity: the yielded value IS the same instance
$o = new stdClass; $o->n = 1;
$vo = vec[stdClass]{$o};
foreach ($vo as $x) { $x->n = 42; var_dump($x === $o); }
echo "o->n = ", $o->n, "\n";

// a nested collection element is itself an immutable collection value
$vv = vec[vec[int]]{vec[int]{7, 8}};
$out = [];
foreach ($vv as $inner) {
    foreach ($inner as $k => $y) { $out[] = "$k:$y"; }
}
echo implode(" ", $out), "\n";
?>
--EXPECT--
array counts after mutation: 2
bool(true)
o->n = 42
0:7 1:8
