--TEST--
contextual collection literals: vec{}/set{}/tuple{} take their element type from context
--EXTENSIONS--
zend_test
--FILE--
<?php

/* vec{...} names the kind but not the element type; the element type comes from
 * the declared type at the use site -- a return type or a typed parameter. The
 * value it builds is identical to the equivalent explicit vec[int]{...}: same
 * canonical type node, so it satisfies the same declarations. Positions with no
 * declared type are a compile error (see contextual_errors.phpt). */

echo "-- return context: vec / set / tuple --\n";
function r_vec(): vec[int]    { return vec{1, 2, 3}; }
function r_set(): set[int]    { return set{1, 2, 2, 3}; }
function r_tuple(): tuple[int, string] { return tuple{1, "a"}; }
printf("vec   count=%d type=%s\n", zend_test_vec_count(r_vec()), get_debug_type(r_vec()));
printf("set   count=%d (deduped)\n", zend_test_vec_count(r_set()));
printf("tuple item1=%s\n", zend_test_vec_get(r_tuple(), 1));

echo "-- argument context: direct / method / static / constructor / dynamic --\n";
function a_vec(vec[int] $v): int { return zend_test_vec_count($v); }
class C {
    public vec[int] $held;
    public function __construct(vec[int] $h) { $this->held = $h; }
    public function m(set[int] $v): int { return zend_test_vec_count($v); }
    public static function s(tuple[int, int] $v): int { return zend_test_vec_get($v, 0); }
}
printf("direct=%d\n", a_vec(vec{1, 2, 3}));
$c = new C(vec{1, 2});
printf("method=%d\n", $c->m(set{5, 5, 6}));
printf("static=%d\n", C::s(tuple{7, 8}));
printf("ctor=%d\n", zend_test_vec_count($c->held));
$dyn = 'a_vec';
printf("dynamic=%d\n", $dyn(vec{9, 9, 9}));

echo "-- variadic target: one type for all --\n";
function variadic(vec[int] ...$vs): int { return count($vs); }
printf("variadic=%d\n", variadic(vec{1}, vec{2, 3}, vec{}));

echo "-- nullable ?vec[int] is usable; empty vec{} is uniform --\n";
function n(?vec[int] $v): string { return $v === null ? 'null' : 'vec'; }
printf("nullable-empty=%s\n", n(vec{}));

echo "-- contextual value is the SAME type node as the explicit literal --\n";
function mk(vec[int] $v): mixed { return $v; }
$fromCtx = mk(vec{1, 2});
$fromExpl = vec[int]{3, 4, 5};
var_dump(zend_test_vec_type_id($fromCtx) === zend_test_vec_type_id($fromExpl));

echo "-- explicit literals are entirely unchanged --\n";
$e = vec[int]{1, 2, 3};
printf("explicit type=%s count=%d\n", get_debug_type($e), zend_test_vec_count($e));

echo "-- vec/set/tuple remain ordinary names --\n";
function vec(int $x): int { return $x * 2; }
const set = 41;
printf("name-fn=%d name-const=%d\n", vec(21), set + 1);
?>
--EXPECT--
-- return context: vec / set / tuple --
vec   count=3 type=collection
set   count=3 (deduped)
tuple item1=a
-- argument context: direct / method / static / constructor / dynamic --
direct=3
method=2
static=7
ctor=2
dynamic=3
-- variadic target: one type for all --
variadic=3
-- nullable ?vec[int] is usable; empty vec{} is uniform --
nullable-empty=vec
-- contextual value is the SAME type node as the explicit literal --
bool(true)
-- explicit literals are entirely unchanged --
explicit type=collection count=3
-- vec/set/tuple remain ordinary names --
name-fn=42 name-const=42
