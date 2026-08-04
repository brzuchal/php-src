--TEST--
Collection intrinsic properties under function JIT: reads must not be shadowed to null
--DESCRIPTION--
The function-JIT non-object property-read cold path (zend_jit_invalid_property_read /
_is) must stay in lockstep with the VM intrinsic-property branch. Without the JIT
fix, a JIT-compiled $collection->count returns null (and warns) instead of the
cardinality, because the terminal cold path shadows the VM handler.
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) die('skip Zend OPcache required');
?>
--INI--
opcache.enable_cli=1
opcache.jit_buffer_size=64M
opcache.jit=1205
opcache.jit_hot_func=2
--FILE--
<?php
// untyped params -> inferred MAY_BE_ANY -> JIT emits the object fast path + cold path
function pc($c)  { return $c->count; }        // FETCH_OBJ_R
function pe($c)  { return $c->isEmpty; }       // FETCH_OBJ_R
function pio($c) { return $c->count ?? -1; }    // FETCH_OBJ_IS (null-coalesce)

$v = vec[int]{1, 2, 3};
$e = vec[int]{};

for ($i = 0; $i < 100000; $i++) {
    pc($v); pe($v); pio($v);
}

var_dump(pc($v));    // 3, not null
var_dump(pe($v));    // false
var_dump(pc($e));    // 0
var_dump(pe($e));    // true
var_dump(pio($v));   // 3
echo "ok\n";
?>
--EXPECT--
int(3)
bool(false)
int(0)
bool(true)
int(3)
ok
