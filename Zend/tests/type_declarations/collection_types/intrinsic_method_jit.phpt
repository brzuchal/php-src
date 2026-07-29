--TEST--
Collection intrinsic methods: function JIT and tracing JIT dispatch correctly
--EXTENSIONS--
opcache
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.jit_buffer_size=64M
opcache.jit=1235
--FILE--
<?php
// A polymorphic (untyped) receiver is JIT-compiled toward the object fast path;
// a collection must deopt to the interpreter instead of raising "method on
// non-object". Object-train, then feed a collection.
class Widget { public function __receiverProbe($v = 0) { return 'obj'; } }
function callProbe($x) { return $x->__receiverProbe(); }
$w = new Widget();
for ($i = 0; $i < 100000; $i++) { callProbe($w); }        // compile toward object

$v = vec[int]{1, 2};
var_dump(callProbe($v) === $v);                            // collection deopts, works

// FCC under JIT:
function makeFcc($x) { return $x->__receiverProbe(...); }
$f = makeFcc($v);
unset($v);
var_dump($f() === $f());
echo "OK\n";
?>
--EXPECT--
bool(true)
bool(true)
OK
