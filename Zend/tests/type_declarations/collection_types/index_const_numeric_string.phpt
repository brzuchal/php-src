--TEST--
vec indexed access: a constant numeric-string key ($v["0"]) is a strict-int miss, not folded to 0
--FILE--
<?php
$v = vec[int]{10, 20, 30};

// A constant "0" is compile-time folded to a long, with the original string kept as an
// extra literal; the collection branch bumps to the string so strict-int rejects it,
// exactly as $v[$k] with a string $k would.
try { var_dump($v["0"]); } catch (\TypeError $e) { echo "read: ", $e->getMessage(), "\n"; }

// isset/?? see it as a miss (not element 0)
var_dump(isset($v["0"]));       // false
var_dump($v["0"] ?? "default"); // "default"
var_dump($v["2"] ?? "default"); // "default"
var_dump(empty($v["0"]));       // true (absent)

// a real int literal still works
var_dump($v[0], isset($v[0]), $v[0] ?? "d");  // 10 true 10
?>
--EXPECT--
read: Collection index must be of type int, string given
bool(false)
string(7) "default"
string(7) "default"
bool(true)
int(10)
bool(true)
int(10)
