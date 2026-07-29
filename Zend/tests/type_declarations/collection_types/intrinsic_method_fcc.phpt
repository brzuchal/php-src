--TEST--
Collection intrinsic methods: first-class callable retains the receiver
--FILE--
<?php
$v = vec[int]{1, 2};
$f = $v->__receiverProbe(...);
unset($v);                         // receiver survives only via the closure

var_dump($f() instanceof \Closure ? 'n/a' : get_debug_type($f()));
var_dump($f());                    // repeated invocation is stable

$g = clone $f;
unset($f);
var_dump($g());                    // clone retains independently

// Object-model isolation: the closure has no bound $this and no static vars.
$rf = new ReflectionFunction($g);
var_dump($rf->getClosureThis() === null);
var_dump($rf->getStaticVariables() === []);
var_dump($g instanceof Closure);

// A closure destroyed without invocation must not leak (checked by --ASAN--/debug).
$v2 = vec[int]{5};
$h = $v2->__receiverProbe(...);
unset($h, $v2);
echo "done\n";
?>
--EXPECT--
string(10) "collection"
vec[int](2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
vec[int](2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
bool(true)
bool(true)
bool(true)
done
