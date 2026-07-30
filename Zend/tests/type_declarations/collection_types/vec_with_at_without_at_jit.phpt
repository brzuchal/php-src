--TEST--
vec: withAt()/withoutAt() dispatch correctly under function JIT and tracing JIT
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
function doWithoutAt($x, $i) { return $x->withoutAt($i); }
class W {
    public function withAt($i, $e) { return 'obj'; }
    public function withoutAt($i) { return 'obj'; }
}
$w = new W();
for ($i = 0; $i < 100000; $i++) { doWithAt($w, 1, 1); doWithoutAt($w, 1); }

$v = vec[int]{10, 20, 30};
var_dump(doWithAt($v, 1, 99)->count === 3);    // collection deopts, works
var_dump(doWithoutAt($v, 0)->count === 2);
var_dump($v->count === 3);                      // unchanged

// FCC under JIT
function mk($x) { return $x->withAt(...); }
$f = mk($v);
unset($v);
var_dump($f(2, 7)->count === 3);
echo "OK\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
OK
