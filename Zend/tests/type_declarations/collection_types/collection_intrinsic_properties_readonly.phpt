--TEST--
Collection intrinsic properties: readonly (writes/compound/inc/dec/unset throw, no mutation)
--FILE--
<?php
$v = vec[int]{1, 2, 3};

$ops = [
    'assign'      => function() use ($v) { $v->count = 1; },
    'compound'    => function() use ($v) { $v->count += 1; },
    'preinc'      => function() use ($v) { ++$v->count; },
    'postinc'     => function() use ($v) { $v->count++; },
    'predec'      => function() use ($v) { --$v->count; },
    'postdec'     => function() use ($v) { $v->count--; },
    'unset'       => function() use ($v) { unset($v->count); },
    'assign_bool' => function() use ($v) { $v->isEmpty = false; },
    'unset_bool'  => function() use ($v) { unset($v->isEmpty); },
];

foreach ($ops as $label => $op) {
    try { $op(); echo "$label: NO THROW\n"; }
    catch (\Error $e) { echo "$label: ", $e->getMessage(), "\n"; }
}

// none of the above mutated the collection
var_dump($v->count);     // still 3
var_dump($v->isEmpty);   // still false
?>
--EXPECT--
assign: Attempt to assign property "count" on collection
compound: Attempt to assign property "count" on collection
preinc: Attempt to increment/decrement property "count" on collection
postinc: Attempt to increment/decrement property "count" on collection
predec: Attempt to increment/decrement property "count" on collection
postdec: Attempt to increment/decrement property "count" on collection
unset: Cannot unset intrinsic property "count" on collection
assign_bool: Attempt to assign property "isEmpty" on collection
unset_bool: Cannot unset intrinsic property "isEmpty" on collection
int(3)
bool(false)
