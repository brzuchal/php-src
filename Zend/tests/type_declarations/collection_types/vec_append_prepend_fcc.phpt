--TEST--
vec: append() first-class callable retains the receiver; each call starts fresh
--FILE--
<?php
$v = vec[int]{1, 2};
$append = $v->append(...);
unset($v);                     // receiver survives only via the closure

var_dump($append(3));          // {1,2,3}
var_dump($append(4));          // {1,2,4} — retained receiver is not mutated

$clone = clone $append;
unset($append);
var_dump($clone(5));           // {1,2,5} — clone retains independently

// binding is rejected on a collection intrinsic FCC
try { $clone->bindTo(new stdClass); } catch (\Error $e) { echo $e->getMessage(), "\n"; }

// destroyed without invocation must not leak (checked by debug/ASAN)
$v2 = vec[int]{9};
$g = $v2->prepend(...);
unset($g, $v2);
echo "done\n";
?>
--EXPECT--
vec[int](3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
vec[int](3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(4)
}
vec[int](3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(5)
}
Cannot bind a native-collection intrinsic method
done
