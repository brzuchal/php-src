--TEST--
set: with()/without() dispatch correctly under function JIT and tracing JIT
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
function doWith($x, $e) { return $x->with($e); }
function doWithout($x, $e) { return $x->without($e); }
class W {
    public function with($e) { return 'obj'; }
    public function without($e) { return 'obj'; }
}
$w = new W();
for ($i = 0; $i < 100000; $i++) { doWith($w, 1); doWithout($w, 1); }

$s = set[int]{1, 2};
var_dump(doWith($s, 3)->count === 3);       // change deopts, works
var_dump(doWith($s, 2)->count === 2);       // no-op deopts, works
var_dump(doWithout($s, 1)->count === 1);
var_dump(doWithout($s, 9)->count === 2);    // no-op
var_dump($s->count === 2);                   // unchanged

// FCC under JIT
function mk($x) { return $x->with(...); }
$f = mk($s);
unset($s);
var_dump($f(3)->count === 3);
echo "OK\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
OK
