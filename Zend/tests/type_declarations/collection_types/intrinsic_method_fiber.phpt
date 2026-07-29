--TEST--
Collection intrinsic methods: FCC created outside, invoked inside a fiber
--FILE--
<?php
$v = tuple[int, string]{1, "x"};
$f = $v->__receiverProbe(...);
unset($v);
$fiber = new Fiber(function () use ($f) {
    Fiber::suspend();
    var_dump($f());
});
$fiber->start();
$fiber->resume();
echo "OK\n";
?>
--EXPECT--
tuple[int,string](2) {
  [0]=>
  int(1)
  [1]=>
  string(1) "x"
}
OK
