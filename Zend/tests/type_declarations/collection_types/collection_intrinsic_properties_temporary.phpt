--TEST--
Collection intrinsic properties: temporaries, function returns, and stored values
--FILE--
<?php
function makeVec()   { return vec[int]{10, 20}; }
function makeEmpty() { return set[int]{}; }

// temporary literal
var_dump((vec[int]{1, 2})->count);     // 2
var_dump((vec[int]{})->isEmpty);       // true

// function-returned value
var_dump(makeVec()->count);            // 2
var_dump(makeEmpty()->isEmpty);        // true

// stored in an array dimension
$arr = ['c' => vec[int]{1, 2, 3}];
var_dump($arr['c']->count);            // 3

// stored in an object property
$o = new stdClass;
$o->c = set[int]{5, 5, 6};
var_dump($o->c->count);                // 2 (dedup)

// a reference to the collection variable itself (not to the intrinsic property)
$v = vec[int]{7, 8, 9, 10};
$r =& $v;
var_dump($r->count);                   // 4
var_dump($r->isEmpty);                 // false
?>
--EXPECT--
int(2)
bool(true)
int(2)
bool(true)
int(3)
int(2)
int(4)
bool(false)
