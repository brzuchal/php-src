--TEST--
Collection intrinsic methods: an intrinsic FCC cannot be $this-rebound
--FILE--
<?php
// A native-collection intrinsic method has no $this; its receiver lives in the
// Closure. Rebinding would strand the closure with neither a receiver nor a
// valid This, so bind/bindTo/call are rejected (previously crashed).
$v = vec[int]{1, 2};
$f = $v->__receiverProbe(...);
foreach (['bindTo' => fn() => $f->bindTo(new stdClass),
          'call'   => fn() => $f->call(new stdClass),
          'bind'   => fn() => Closure::bind($f, new stdClass)] as $op => $fn) {
    try { $fn(); echo "$op: NOT REJECTED\n"; }
    catch (\Error $e) { echo "$op: ", $e->getMessage(), "\n"; }
}
// The original callable is unaffected and still works:
var_dump($f() === $v);
?>
--EXPECT--
bindTo: Cannot bind a native-collection intrinsic method
call: Cannot bind a native-collection intrinsic method
bind: Cannot bind a native-collection intrinsic method
bool(true)
