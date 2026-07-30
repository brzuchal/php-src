--TEST--
vec/tuple indexed access: isset()/empty()/?? are total (never throw) and strict-int
--FILE--
<?php
$v = vec[int]{10, 0, 30};   // element 1 is falsy (0)
$t = tuple[int, string]{5, ""};
$s = set[int]{1, 2};

// isset: in-range && non-null -> true; out-of-range / non-int / set -> false (no throw)
var_dump(isset($v[0]), isset($v[1]), isset($v[2]));       // true true true (0 is set, just falsy)
var_dump(isset($v[3]), isset($v[-1]), isset($v[99]));     // false false false
var_dump(isset($v["0"]), isset($v[1.0]), isset($v[true])); // false (strict int)
var_dump(isset($s[0]));                                    // false (set not indexable)

// empty: absent OR falsy -> true
var_dump(empty($v[0]), empty($v[1]), empty($v[3]));        // false true true
var_dump(empty($t[1]), empty($t[0]));                      // true ("") false (5)

// ?? uses the silent (BP_VAR_IS) read: a miss yields the default, never throws
var_dump($v[1] ?? "d", $v[3] ?? "d", $v[-1] ?? "d", $v["0"] ?? "d");  // 0 d d d
var_dump($s[0] ?? "d");                                    // d (set: silent miss under ??)

// none of this mutates the receivers
var_dump($v->count === 3, $t->count === 2, $s->count === 2);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
int(0)
string(1) "d"
string(1) "d"
string(1) "d"
string(1) "d"
bool(true)
bool(true)
bool(true)
