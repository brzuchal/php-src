--TEST--
vec: append() FCC created outside, invoked inside a fiber
--FILE--
<?php
$v = vec[int]{1, 2};
$f = $v->append(...);
unset($v);
$fiber = new Fiber(function () use ($f) {
    Fiber::suspend();
    var_dump($f(3));
});
$fiber->start();
$fiber->resume();
echo "OK\n";
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
OK
