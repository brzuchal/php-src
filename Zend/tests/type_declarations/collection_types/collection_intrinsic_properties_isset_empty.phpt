--TEST--
Collection intrinsic properties: isset()/empty() and the count-vs-empty distinction
--FILE--
<?php
$v = vec[int]{1, 2, 3};
$e = vec[int]{};

// isset(): known intrinsic properties are always set; unknown is not
var_dump(isset($v->count));      // true
var_dump(isset($v->isEmpty));    // true
var_dump(isset($v->unknown));    // false
var_dump(isset($e->count));      // true (value 0, but the property exists)

// The load-bearing distinction: empty($collection) vs empty($collection->count)
var_dump(empty($e));             // false — a collection is always truthy
var_dump(empty($e->count));      // true  — truthiness of the int 0
var_dump(empty($e->isEmpty));    // false — truthiness of the bool true
var_dump(empty($v->count));      // false — int 3
var_dump(empty($v->unknown));    // true  — unknown intrinsic is absent (no throw)

// null-coalesce uses the isset path: known yields the value, unknown yields the fallback
var_dump($v->count ?? -1);       // 3
var_dump($v->unknown ?? -1);     // -1
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
bool(true)
int(3)
int(-1)
