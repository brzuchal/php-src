--TEST--
collection types: vec lifecycle, refcounting and cycle collection
--EXTENSIONS--
zend_test
--FILE--
<?php
class Foo { public $v; }

/* 1. direct cycle: vec -> object -> vec */
gc_collect_cycles();
for ($i = 0; $i < 3; $i++) {
    $o = new Foo();
    zend_test_make_vec([$o], 'Foo', $vec);
    $o->v = $vec;
    unset($o, $vec);
}
var_dump(gc_collect_cycles() > 0);

/* 2. nested cycle: outer vec -> inner vec -> object -> outer vec */
gc_collect_cycles();
$o = new Foo();
zend_test_make_vec([$o], 'Foo', $inner);
zend_test_make_vec([$inner], 'vec:Foo', $outer);
$o->v = $outer;
var_dump(zend_test_vec_count($outer) === 1, zend_test_vec_count($inner) === 1);
unset($o, $inner, $outer);
var_dump(gc_collect_cycles() > 0);

/* 3. a vec of ordinary arrays: elements stay mutable, structure does not */
zend_test_make_vec([[1, 2], [3]], 'array', $va);
var_dump(zend_test_vec_count($va) === 2, zend_test_vec_get($va, 0) === [1, 2]);

/* 4. stored inside arrays and through references */
zend_test_make_vec([1, 2, 3], 'int', $v);
$holder = ['k' => $v];
$ref = &$v;
var_dump(zend_test_vec_count($holder['k']) === 3, zend_test_vec_count($ref) === 3);
unset($ref, $holder);

/* 5. ordinary scalar refcount churn */
$base = zend_test_refcount($v);
$copy = $v;
var_dump(zend_test_refcount($v) === $base + 1);
unset($copy);
var_dump(zend_test_refcount($v) === $base);
var_dump(zend_test_vec_get($v, 1) === 2);

/* 6. partial construction failure releases only installed elements */
try { zend_test_make_vec([1, 'x', 3], 'int', $bad); }
catch (ValueError $e) { echo "partial: rejected\n"; }
try { zend_test_make_vec([new Foo()], 'int', $bad2); }
catch (ValueError $e) { echo "partial: rejected\n"; }

/* 7. matching and mismatching nested element descriptors */
zend_test_make_vec([1], 'int', $vi);
zend_test_make_vec([$vi], 'vec:int', $okNested);
var_dump(zend_test_vec_count($okNested) === 1);
zend_test_make_vec([new Foo()], 'Foo', $vf);
try { zend_test_make_vec([$vf], 'vec:int', $badNested); }
catch (ValueError $e) { echo "nested mismatch: rejected\n"; }
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
partial: rejected
partial: rejected
bool(true)
nested mismatch: rejected
done
