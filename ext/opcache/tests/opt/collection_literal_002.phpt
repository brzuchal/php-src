--TEST--
Collection literal 002: vec builder takes constant element operands directly; outer array unchanged
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
/* Every element is a constant: the vec builder stores each one directly via an
 * ADD_COLLECTION_ELEMENT with a CONST operand (no element array to fold). The outer
 * array [$a, $b, $c] is ordinary and still uses INIT_ARRAY -- only vec took the new path. */
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
     ; (lines=17, args=0, vars=3, tmps=1)
     ; (after optimizer)
     ; %scollection_literal_002.php:5-10
0000 T3 = INIT_COLLECTION 0 3
0001 T3 = ADD_COLLECTION_ELEMENT int(1)
0002 T3 = ADD_COLLECTION_ELEMENT int(2)
0003 T3 = ADD_COLLECTION_ELEMENT int(3)
0004 CV0($a) = FINISH_COLLECTION 0 T3
0005 T3 = INIT_COLLECTION 1 2
0006 T3 = ADD_COLLECTION_ELEMENT string("aa")
0007 T3 = ADD_COLLECTION_ELEMENT string("bb")
0008 CV1($b) = FINISH_COLLECTION 1 T3
0009 T3 = INIT_COLLECTION 2 2
0010 T3 = ADD_COLLECTION_ELEMENT array(...)
0011 T3 = ADD_COLLECTION_ELEMENT array(...)
0012 CV2($c) = FINISH_COLLECTION 2 T3
0013 T3 = INIT_ARRAY 3 (packed) CV0($a) NEXT
0014 T3 = ADD_ARRAY_ELEMENT CV1($b) NEXT
0015 T3 = ADD_ARRAY_ELEMENT CV2($c) NEXT
0016 RETURN T3
LIVE RANGES:
     3: 0001 - 0004 (tmp/var)
     3: 0006 - 0008 (tmp/var)
     3: 0010 - 0012 (tmp/var)
     3: 0014 - 0016 (tmp/var)
