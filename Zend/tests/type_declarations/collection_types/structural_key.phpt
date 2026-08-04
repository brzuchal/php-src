--TEST--
Collection types: structural hashing key contract
--EXTENSIONS--
zend_test
--FILE--
<?php

/* The contract under test:
 *   zend_type_structurally_equals(a, b)  =>  hash(a) == hash(b)
 * The converse is not claimed, so unequal keys below prove distinctness only
 * as far as this implementation goes; equal keys are the load-bearing part. */

function same_mask_a(vec[int] $x): void {}
function same_mask_b(vec[int] $x): void {}
function other_mask(vec[string] $x): void {}
function nullable_mask(vec[?int] $x): void {}

function class_a(vec[Foo] $x): void {}
function class_b(vec[Foo] $x): void {}
function class_upper(vec[FOO] $x): void {}
function class_mixed_case(vec[fOo] $x): void {}

function nested_a(vec[vec[int]] $x): void {}
function nested_b(vec[vec[int]] $x): void {}
function nested_deeper(vec[vec[vec[int]]] $x): void {}
function nested_other(vec[vec[string]] $x): void {}

$key = 'zend_test_collection_key';

echo "-- identical scalar masks --\n";
var_dump($key('same_mask_a') === $key('same_mask_b'));

echo "-- different scalar masks --\n";
var_dump($key('same_mask_a') === $key('other_mask'));
var_dump($key('same_mask_a') === $key('nullable_mask'));

echo "-- identical class names, separate declarations --\n";
var_dump($key('class_a') === $key('class_b'));

echo "-- class-name case normalization --\n";
var_dump($key('class_a') === $key('class_upper'));
var_dump($key('class_a') === $key('class_mixed_case'));

echo "-- nested descriptors --\n";
var_dump($key('nested_a') === $key('nested_b'));
var_dump($key('nested_a') === $key('nested_other'));

echo "-- different nesting depth --\n";
var_dump($key('same_mask_a') === $key('nested_a'));
var_dump($key('nested_a') === $key('nested_deeper'));

echo "-- ordered positional members --\n";
/* No kind with arity 2 has syntax yet, so these are built directly; the two
 * descriptors differ only in member order. */
[$first, $second] = zend_test_collection_key_positional();
var_dump($first === $second);

echo "-- provenance bits ignored --\n";
/* Same descriptor hashed twice, differing only in _ZEND_TYPE_ARENA_BIT. */
[$as_arena, $as_heap] = zend_test_collection_key_provenance('same_mask_a');
var_dump($as_arena === $as_heap);

echo "-- supported-input boundary --\n";
var_dump(zend_test_collection_key_supported('same_mask_a'));
var_dump(zend_test_collection_key_supported('nested_deeper'));
var_dump(zend_test_collection_key_unsupported());

echo "-- unsupported input is refused, not keyed --\n";
try {
    zend_test_collection_key('undefined_function_name');
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECT--
-- identical scalar masks --
bool(true)
-- different scalar masks --
bool(false)
bool(false)
-- identical class names, separate declarations --
bool(true)
-- class-name case normalization --
bool(true)
bool(true)
-- nested descriptors --
bool(true)
bool(false)
-- different nesting depth --
bool(false)
bool(false)
-- ordered positional members --
bool(false)
-- provenance bits ignored --
bool(true)
-- supported-input boundary --
bool(true)
bool(true)
array(3) {
  ["union_member"]=>
  bool(false)
  ["zero_arity"]=>
  bool(false)
  ["non_collection_root"]=>
  bool(false)
}
-- unsupported input is refused, not keyed --
zend_test_collection_key(): Argument #1 ($function) must name a function with at least one parameter
