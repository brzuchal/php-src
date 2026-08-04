--TEST--
collection values: strict === / !== against null, false and true
--FILE--
<?php

/* A collection value is never identical to null, false or true. So for every
 * kind (vec/tuple/set), empty or not, in either operand order, and in every
 * context (direct expression, variable, function argument, return value, branch
 * condition, loop): $c === null|false|true is false and $c !== null|false|true
 * is true. The answer must not depend on opcache, optimization level or JIT.
 *
 * These lower to ZEND_TYPE_CHECK with a type-code bitmask; IS_COLLECTION sits
 * outside that mask's scalar universe, so the "!==" complement historically
 * excluded collections and returned the wrong answer. See
 * implementation-notes/a5-type-check-collision.md. */

function kinds(): array {
    return [
        'vec empty' => vec[int]{},
        'vec'       => vec[int]{1, 2, 3},
        'tuple'     => tuple[int, string]{1, "a"},
        'set empty' => set[int]{},
        'set'       => set[int]{1, 2, 3},
    ];
}

echo "-- === (want 0) and !== (want 1) vs null/false/true, both operand orders --\n";
foreach (kinds() as $label => $c) {
    printf("%-10s| =N:%d N=:%d !N:%d N!:%d | =F:%d F=:%d !F:%d F!:%d | =T:%d T=:%d !T:%d T!:%d\n",
        $label,
        $c === null, null === $c, $c !== null, null !== $c,
        $c === false, false === $c, $c !== false, false !== $c,
        $c === true, true === $c, $c !== true, true !== $c);
}

echo "-- branch / argument / return-value contexts --\n";
function isNotNull($x): bool { return $x !== null; }
function isNull($x): bool { return $x === null; }
$c = vec[int]{1};
var_dump(isNotNull($c), isNull($c));
echo ($c !== null) ? "branch !==null: taken\n" : "branch !==null: NOT taken (BUG)\n";
echo ($c === null) ? "branch ===null: taken (BUG)\n" : "branch ===null: not taken\n";
echo "ternary !==false: " . (($c !== false) ? "yes" : "no") . "\n";

echo "-- inline literal (element array is const-folded, value built at runtime) --\n";
var_dump(vec[int]{1, 2, 3} !== null);
var_dump(vec[int]{1, 2, 3} === null);
var_dump(tuple[int, int]{1, 2} !== true);
var_dump(set[string]{"a"} !== false);

echo "-- repeated execution stays correct (ages inference/JIT caches) --\n";
$c = set[string]{"a", "b"};
$ok = true;
for ($i = 0; $i < 100000; $i++) {
    if ($c === null || $c === false || $c === true) { $ok = false; break; }
    if (!($c !== null) || !($c !== false) || !($c !== true)) { $ok = false; break; }
}
var_dump($ok);
?>
--EXPECT--
-- === (want 0) and !== (want 1) vs null/false/true, both operand orders --
vec empty | =N:0 N=:0 !N:1 N!:1 | =F:0 F=:0 !F:1 F!:1 | =T:0 T=:0 !T:1 T!:1
vec       | =N:0 N=:0 !N:1 N!:1 | =F:0 F=:0 !F:1 F!:1 | =T:0 T=:0 !T:1 T!:1
tuple     | =N:0 N=:0 !N:1 N!:1 | =F:0 F=:0 !F:1 F!:1 | =T:0 T=:0 !T:1 T!:1
set empty | =N:0 N=:0 !N:1 N!:1 | =F:0 F=:0 !F:1 F!:1 | =T:0 T=:0 !T:1 T!:1
set       | =N:0 N=:0 !N:1 N!:1 | =F:0 F=:0 !F:1 F!:1 | =T:0 T=:0 !T:1 T!:1
-- branch / argument / return-value contexts --
bool(true)
bool(false)
branch !==null: taken
branch ===null: not taken
ternary !==false: yes
-- inline literal (element array is const-folded, value built at runtime) --
bool(true)
bool(false)
bool(true)
bool(true)
-- repeated execution stays correct (ages inference/JIT caches) --
bool(true)
