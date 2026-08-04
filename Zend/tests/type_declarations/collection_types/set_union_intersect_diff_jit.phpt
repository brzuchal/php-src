--TEST--
set: union()/intersect()/diff() dispatch correctly under function JIT and tracing JIT
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
function doOp($x, $m, $o) { return $x->$m($o); }
class W {
    public function union($o) { return 'obj'; }
    public function intersect($o) { return 'obj'; }
    public function diff($o) { return 'obj'; }
}
$w = new W();
for ($i = 0; $i < 100000; $i++) { doOp($w, 'union', 1); doOp($w, 'intersect', 1); doOp($w, 'diff', 1); }

$a = set[int]{1, 2, 3};
$b = set[int]{3, 4, 5};
var_dump(doOp($a, 'union', $b)->count === 5);       // change deopts, works
var_dump(doOp($a, 'intersect', $b)->count === 1);
var_dump(doOp($a, 'diff', $b)->count === 2);
var_dump(doOp($a, 'union', set[int]{1})->count === 3);  // no-op deopts, works
var_dump($a->count === 3);                           // unchanged

// FCC under JIT
function mk($x) { return $x->union(...); }
$f = mk($a);
unset($a);
var_dump($f(set[int]{9})->count === 4);
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
