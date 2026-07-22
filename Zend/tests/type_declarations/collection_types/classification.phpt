--TEST--
Collection types: cached classification metadata on canonical nodes
--EXTENSIONS--
zend_test
--FILE--
<?php

function flat_int(vec[int] $x): void {}
function flat_str(vec[string] $x): void {}
function flat_cls(vec[Foo] $x): void {}
function nest1(vec[vec[int]] $x): void {}
function nest2(vec[vec[vec[int]]] $x): void {}
function nest_cls(vec[vec[Foo]] $x): void {}

$c = 'zend_test_collection_classify';

echo "-- flat builtin --\n";
$f = $c('flat_int');
var_dump($f['depth'], $f['has_nested'], $f['all_mask_members'], $f['value_constructible']);
var_dump($f['fast_mask'] !== 0);

echo "-- flat class name --\n";
$f = $c('flat_cls');
var_dump($f['depth'], $f['has_class_name'], $f['all_mask_members']);
/* A class element needs more than a mask test. */
var_dump($f['fast_mask'] === 0);

echo "-- depth accumulates from cached child depth --\n";
var_dump($c('nest1')['depth'], $c('nest2')['depth']);

echo "-- class-name presence propagates transitively --\n";
var_dump($c('nest1')['has_class_name'], $c('nest_cls')['has_class_name']);

echo "-- nesting disables the mask fast path --\n";
var_dump($c('nest1')['has_nested'], $c('nest1')['all_mask_members'], $c('nest1')['fast_mask']);

echo "-- nodes are immutable: repeated reads are identical --\n";
$before = $c('nest2');
zend_test_make_vec([1], 'int', $v1);
zend_test_make_vec([$v1], 'vec:int', $v2);
zend_test_make_vec([], 'string', $v3);
$after = $c('nest2');
var_dump($before === $after);

echo "-- value constructibility is cached, not re-derived --\n";
var_dump($c('flat_int')['value_constructible']);
var_dump($c('nest1')['value_constructible']);
var_dump($c('flat_cls')['value_constructible']);

echo "-- unsupported forms are rejected before promotion --\n";
var_dump(zend_test_collection_key_unsupported());

?>
--EXPECT--
-- flat builtin --
int(1)
bool(false)
bool(true)
bool(true)
bool(true)
-- flat class name --
int(1)
bool(true)
bool(false)
bool(true)
-- depth accumulates from cached child depth --
int(2)
int(3)
-- class-name presence propagates transitively --
bool(false)
bool(true)
-- nesting disables the mask fast path --
bool(true)
bool(false)
int(0)
-- nodes are immutable: repeated reads are identical --
bool(true)
-- value constructibility is cached, not re-derived --
bool(true)
bool(true)
bool(true)
-- unsupported forms are rejected before promotion --
array(3) {
  ["union_member"]=>
  bool(false)
  ["zero_arity"]=>
  bool(false)
  ["non_collection_root"]=>
  bool(false)
}
