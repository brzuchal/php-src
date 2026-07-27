--TEST--
Bare collection-kind types vec[]/set[]/tuple[]: parameter acceptance and rejection
--FILE--
<?php
function acceptVec(vec[] $v): string { return "ok"; }
function acceptSet(set[] $v): string { return "ok"; }
function acceptTuple(tuple[] $v): string { return "ok"; }

echo acceptVec(vec[int]{1, 2}), "\n";          // any element type accepted
echo acceptVec(vec[string]{"a"}), "\n";
echo acceptSet(set[int]{1, 2}), "\n";
echo acceptTuple(tuple[int, string]{1, "a"}), "\n";

foreach ([
    'wrong-kind' => fn() => acceptVec(set[int]{1}),
    'array'      => fn() => acceptVec([1, 2]),
    'int'        => fn() => acceptVec(5),
    'null'       => fn() => acceptVec(null),
] as $label => $fn) {
    try { $fn(); echo "$label: NO THROW\n"; }
    catch (\TypeError $e) { echo "$label: ", $e->getMessage(), "\n"; }
}
?>
--EXPECTF--
ok
ok
ok
ok
wrong-kind: acceptVec(): Argument #1 ($v) must be of type vec[], set[int] given, called in %s on line %d
array: acceptVec(): Argument #1 ($v) must be of type vec[], array given, called in %s on line %d
int: acceptVec(): Argument #1 ($v) must be of type vec[], int given, called in %s on line %d
null: acceptVec(): Argument #1 ($v) must be of type vec[], null given, called in %s on line %d
