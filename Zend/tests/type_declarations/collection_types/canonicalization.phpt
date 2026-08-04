--TEST--
Collection types: runtime type canonicalization
--EXTENSIONS--
zend_test
--FILE--
<?php

function a_int(vec[int] $x): void {}
function b_int(vec[int] $x): void {}
function a_string(vec[string] $x): void {}
function a_int_nullable(?vec[int] $x): void {}
function a_int_member_nullable(vec[?int] $x): void {}

function a_foo(vec[Foo] $x): void {}
function b_foo(vec[FOO] $x): void {}
function c_foo(vec[fOo] $x): void {}
function a_bar(vec[Bar] $x): void {}

function a_nested(vec[vec[int]] $x): void {}
function b_nested(vec[vec[int]] $x): void {}
function other_nested(vec[vec[string]] $x): void {}

$intern = 'zend_test_collection_intern';

echo "-- identical descriptors canonicalize to one node --\n";
var_dump($intern('a_int')['id'] === $intern('b_int')['id']);

echo "-- different descriptors do not --\n";
var_dump($intern('a_int')['id'] === $intern('a_string')['id']);
var_dump($intern('a_foo')['id'] === $intern('a_bar')['id']);

echo "-- root nullability is not part of descriptor identity --\n";
/* ?vec[int] and vec[int] share one node: the root mask lives in the outer
 * zend_type and is enforced by the ordinary mask check, never by the matcher,
 * so the key must not fold it either. Member nullability stays semantic. */
var_dump($intern('a_int')['id'] === $intern('a_int_nullable')['id']);
var_dump($intern('a_int')['id'] === $intern('a_int_member_nullable')['id']);

echo "-- repeated promotion returns the identical pointer --\n";
$first = $intern('a_int')['id'];
for ($i = 0; $i < 5; $i++) {
    if ($intern('a_int')['id'] !== $first) {
        echo "UNSTABLE\n";
    }
}
var_dump($intern('a_int')['id'] === $first);

echo "-- class names canonicalize case-insensitively --\n";
var_dump($intern('a_foo')['id'] === $intern('b_foo')['id']);
var_dump($intern('a_foo')['id'] === $intern('c_foo')['id']);

echo "-- nested descriptors reuse the child node --\n";
$nested = $intern('a_nested');
var_dump($nested['id'] === $intern('b_nested')['id']);
/* The inner vec[int] must be the very same node a top-level vec[int] gets. */
var_dump($nested['child_id'] === $intern('a_int')['id']);
var_dump($nested['child_id'] !== $intern('other_nested')['child_id']);
var_dump($nested['name']);

echo "-- compiler arena descriptors do not escape --\n";
$probe = $intern('a_nested');
/* Without opcache the declared type lives in the compiler's request arena; with
 * opcache the op_array (and its arg_info) is persisted into shared memory, so the
 * arena bit is stripped. Either provenance is fine here -- what this section
 * proves is that the canonical node below did not escape from the declaration. */
$declared_in_arena = !(extension_loaded('Zend OPcache') && (bool) ini_get('opcache.enable_cli'));
var_dump($probe['declared_type_uses_arena'] === $declared_in_arena);
/* ...yet the node is a distinct allocation, and no member kept provenance. */
var_dump($probe['aliases_descriptor']);
var_dump($probe['members_arena_free']);

echo "-- runtime values borrow the canonical node --\n";
zend_test_make_vec([1, 2, 3], 'int', $vec);
zend_test_make_vec([4], 'int', $vec2);
zend_test_make_vec(['s'], 'string', $vec3);
var_dump(zend_test_vec_type_id($vec) === zend_test_vec_type_id($vec2));
var_dump(zend_test_vec_type_id($vec) === zend_test_vec_type_id($vec3));
/* The value's node is the same one the declared type promotes to. */
var_dump(zend_test_vec_type_id($vec) === $intern('a_int')['id']);

echo "-- values outlive nothing they borrow --\n";
unset($vec2);
var_dump(zend_test_vec_type_id($vec) === $intern('a_int')['id']);

echo "-- collisions keep distinct types distinct --\n";
var_dump(zend_test_collection_collision_selftest());

?>
--EXPECT--
-- identical descriptors canonicalize to one node --
bool(true)
-- different descriptors do not --
bool(false)
bool(false)
-- root nullability is not part of descriptor identity --
bool(true)
bool(false)
-- repeated promotion returns the identical pointer --
bool(true)
-- class names canonicalize case-insensitively --
bool(true)
bool(true)
-- nested descriptors reuse the child node --
bool(true)
bool(true)
bool(true)
string(13) "vec[vec[int]]"
-- compiler arena descriptors do not escape --
bool(true)
bool(false)
bool(true)
-- runtime values borrow the canonical node --
bool(true)
bool(false)
bool(true)
-- values outlive nothing they borrow --
bool(true)
-- collisions keep distinct types distinct --
bool(true)
