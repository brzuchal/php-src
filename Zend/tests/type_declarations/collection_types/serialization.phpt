--TEST--
collection values: serialize / unserialize round-trip and wire format
--EXTENSIONS--
zend_test
--FILE--
<?php

/* The wire format is an extension of serialize()'s token grammar: token L,
 * kind by name, builtin members as serialize's own letters (i/d/s/b/a), C: for
 * class members, L: for nested collections. See
 * implementation-notes/collection-serialization-format.md. */

echo "-- exact serialized form --\n";
class Foo {}
$forms = [
    'vec int'      => vec[int]{1, 2, 3},
    'vec empty'    => vec[int]{},
    'tuple'        => tuple[int, string]{1, 'a'},
    'set'          => set[string]{'a', 'b'},
    'vec float'    => vec[float]{1.5},
    'vec bool'     => vec[bool]{true, false},
    'vec array'    => vec[array]{[1]},
    'vec class'    => vec[Foo]{new Foo},
    'vec nested'   => vec[vec[int]]{ vec[int]{1} },
];
foreach ($forms as $label => $v) {
    printf("%-11s %s\n", $label, serialize($v));
}

echo "-- round-trips to an equal, same-typed value --\n";
function roundtrips($v): string {
    $u = unserialize(serialize($v));
    if (!($u instanceof stdClass) === false && $u === null) return 'null';
    $sameType = zend_test_vec_type_id($u) === zend_test_vec_type_id($v);
    $sameCount = zend_test_vec_count($u) === zend_test_vec_count($v);
    return ($sameType && $sameCount) ? 'ok' : 'MISMATCH';
}
foreach ($forms as $label => $v) {
    printf("%-11s %s\n", $label, roundtrips($v));
}

echo "-- element values survive exactly --\n";
$v = vec[string]{"a\nb", "c\"d", ""};
$u = unserialize(serialize($v));
var_dump(zend_test_vec_get($u, 0) === "a\nb",
         zend_test_vec_get($u, 1) === "c\"d",
         zend_test_vec_get($u, 2) === "");

$v = vec[float]{1.5, 0.0, -2.25};
$u = unserialize(serialize($v));
var_dump(zend_test_vec_get($u, 2));

echo "-- set dedups on the way in and out --\n";
$s = unserialize(serialize(set[int]{1, 2, 2, 3, 1}));
var_dump(zend_test_vec_count($s));

echo "-- collections nested in arrays/objects --\n";
$graph = ['v' => vec[int]{1, 2}, 't' => tuple[int, int]{3, 4}];
$u = unserialize(serialize($graph));
var_dump(zend_test_vec_count($u['v']), zend_test_vec_get($u['t'], 1));

echo "-- a shared object element keeps its identity across the graph --\n";
$o = new Foo();
$u = unserialize(serialize([vec[Foo]{$o}, $o]));
var_dump(zend_test_vec_get($u[0], 0) === $u[1]);

?>
--EXPECT--
-- exact serialized form --
vec int     L:vec:1:{i;}:3:{i:1;i:2;i:3;}
vec empty   L:vec:1:{i;}:0:{}
tuple       L:tuple:2:{i;s;}:2:{i:1;s:1:"a";}
set         L:set:1:{s;}:2:{s:1:"a";s:1:"b";}
vec float   L:vec:1:{d;}:1:{d:1.5;}
vec bool    L:vec:1:{b;}:2:{b:1;b:0;}
vec array   L:vec:1:{a;}:1:{a:1:{i:0;i:1;}}
vec class   L:vec:1:{c:3:"Foo";}:1:{O:3:"Foo":0:{}}
vec nested  L:vec:1:{l:vec:1:{i;};}:1:{L:vec:1:{i;}:1:{i:1;}}
-- round-trips to an equal, same-typed value --
vec int     ok
vec empty   ok
tuple       ok
set         ok
vec float   ok
vec bool    ok
vec array   ok
vec class   ok
vec nested  ok
-- element values survive exactly --
bool(true)
bool(true)
bool(true)
float(-2.25)
-- set dedups on the way in and out --
int(3)
-- collections nested in arrays/objects --
int(2)
int(4)
-- a shared object element keeps its identity across the graph --
bool(true)
