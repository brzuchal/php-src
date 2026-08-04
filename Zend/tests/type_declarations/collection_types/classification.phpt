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
var_dump($f['all_mask_members'], $f['value_constructible']);
var_dump($f['fast_mask'] !== 0);

echo "-- flat class name --\n";
$f = $c('flat_cls');
var_dump($f['all_mask_members']);
/* A class element needs more than a mask test. */
var_dump($f['fast_mask'] === 0);

echo "-- nesting disables the mask fast path --\n";
var_dump($c('nest1')['all_mask_members'], $c('nest1')['fast_mask']);
var_dump($c('nest2')['all_mask_members'], $c('nest_cls')['all_mask_members']);

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
bool(true)
bool(true)
bool(true)
-- flat class name --
bool(false)
bool(true)
-- nesting disables the mask fast path --
bool(false)
int(0)
bool(false)
bool(false)
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
