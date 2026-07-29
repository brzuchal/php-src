--TEST--
Collection intrinsic methods: __receiverProbe direct dispatch (all kinds, args)
--FILE--
<?php
// __receiverProbe is a private, test-only intrinsic: it returns its receiver
// unchanged. It exercises the Model D header-slot receiver transport.
$v = vec[int]{1, 2};
$r = $v->__receiverProbe();
var_dump($r === $v);
var_dump(get_debug_type($r));
var_dump($r);

$s = set[string]{"a", "b"};
var_dump($s->__receiverProbe() === $s);

$t = tuple[int, string]{1, "x"};
var_dump($t->__receiverProbe() === $t);

// Public parameter #1 ($value) never affects the receiver:
var_dump($v->__receiverProbe(42) === $v);
var_dump($v->__receiverProbe(value: 42) === $v);

// Temporary (non-CV) receiver from a call result:
function make(): vec[int] { return vec[int]{7, 8, 9}; }
var_dump(make()->__receiverProbe());
?>
--EXPECT--
bool(true)
string(10) "collection"
vec[int](2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
bool(true)
bool(true)
bool(true)
bool(true)
vec[int](3) {
  [0]=>
  int(7)
  [1]=>
  int(8)
  [2]=>
  int(9)
}
