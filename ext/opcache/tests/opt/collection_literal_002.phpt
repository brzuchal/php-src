--TEST--
Collection literal 002: an all-constant element list folds to a constant operand
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.optimization_level=-1
opcache.opt_debug_level=0x20000
opcache.preload=
zend_test.observer.enabled=0
--EXTENSIONS--
opcache
--FILE--
<?php
/* The common shape in real code: every element is a constant, so the optimizer
 * folds the element array away and CONSTRUCT_COLLECTION reads a CONST operand.
 * That is why the handler accepts one -- it is the usual case, not a corner. */
function lit() {
    $a = vec[int]{1, 2, 3};
    $b = vec[string]{'aa', 'bb'};
    $c = vec[array]{[1, 2], ['k' => 'v']};
    return [$a, $b, $c];
}
?>
--EXPECTF--
$_main:
     ; (lines=1, args=0, vars=0, tmps=0)
     ; (after optimizer)
     ; %scollection_literal_002.php:1-12
0000 RETURN int(1)

lit:
     ; (lines=7, args=0, vars=3, tmps=1)
     ; (after optimizer)
     ; %scollection_literal_002.php:5-10
0000 CV0($a) = CONSTRUCT_COLLECTION 0 array(...)
0001 CV1($b) = CONSTRUCT_COLLECTION 1 array(...)
0002 CV2($c) = CONSTRUCT_COLLECTION 2 array(...)
0003 T3 = INIT_ARRAY 3 (packed) CV0($a) NEXT
0004 T3 = ADD_ARRAY_ELEMENT CV1($b) NEXT
0005 T3 = ADD_ARRAY_ELEMENT CV2($c) NEXT
0006 RETURN T3
LIVE RANGES:
     3: 0004 - 0006 (tmp/var)
