--TEST--
tuple: withAt() dispatches correctly under function JIT and tracing JIT
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
function doWithAt($x, $i, $e) { return $x->withAt($i, $e); }
class W { public function withAt($i, $e) { return 'obj'; } }
$w = new W();
for ($i = 0; $i < 100000; $i++) { doWithAt($w, 0, 1); }

$t = tuple[int, string]{1, "a"};
var_dump(doWithAt($t, 0, 9)->count === 2);   // collection deopts, works
var_dump(doWithAt($t, 1, "b")->count === 2);
var_dump($t->count === 2);                    // unchanged

// FCC under JIT
function mk($x) { return $x->withAt(...); }
$f = mk($t);
unset($t);
var_dump($f(0, 7)->count === 2);
echo "OK\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
OK
