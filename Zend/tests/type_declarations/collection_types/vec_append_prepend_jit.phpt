--TEST--
vec: append()/prepend() dispatch correctly under function JIT and tracing JIT
--EXTENSIONS--
opcache
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.jit_buffer_size=64M
opcache.jit=1235
--FILE--
<?php
// Untyped receiver -> object fast path; a collection must deopt to the
// interpreter rather than raise "method on non-object".
function doAppend($x, $e) { return $x->append($e); }
class W { public function append($e) { return 'obj'; } }
$w = new W();
for ($i = 0; $i < 100000; $i++) { doAppend($w, 1); }   // compile toward object

$v = vec[int]{1, 2};
var_dump(doAppend($v, 3)->count === 3);                // collection deopts, works
var_dump($v->count === 2);                             // unchanged

// FCC under JIT
function mk($x) { return $x->append(...); }
$f = mk($v);
unset($v);
var_dump($f(3)->count === 3);
echo "OK\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
OK
