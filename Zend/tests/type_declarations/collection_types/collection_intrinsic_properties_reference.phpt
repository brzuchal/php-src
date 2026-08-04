--TEST--
Collection intrinsic properties: by-reference acquisition is rejected
--FILE--
<?php
$v = vec[int]{1, 2, 3};

// direct reference assignment
try { $r =& $v->count; echo "ref-assign: NO THROW\n"; }
catch (\Error $e) { echo "ref-assign: ", get_class($e), "\n"; }

// by-reference argument
function byRef(&$x) { $x = 99; }
try { byRef($v->count); echo "by-ref-arg: NO THROW\n"; }
catch (\Error $e) { echo "by-ref-arg: ", get_class($e), "\n"; }

// by-reference to a property of a collection stored in an array element
$arr = [vec[int]{1, 2}];
try { $r2 =& $arr[0]->count; echo "nested-ref: NO THROW\n"; }
catch (\Error $e) { echo "nested-ref: ", get_class($e), "\n"; }

var_dump($v->count);   // unchanged
?>
--EXPECT--
ref-assign: Error
by-ref-arg: Error
nested-ref: Error
int(3)
