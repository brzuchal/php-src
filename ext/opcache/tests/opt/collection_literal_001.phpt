--TEST--
Collection literal 001: elements are built as an array, then constructed once
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
function lit(int $n) {
    $a = vec[int]{};
    $b = vec[int]{$n, $n + 1};
    $c = vec[vec[int]]{$b};
    return $c;
}
?>
--EXPECTF--
$_main:
     ; (lines=1, args=0, vars=0, tmps=0)
     ; (after optimizer)
     ; %scollection_literal_001.php:1-9
0000 RETURN int(1)

lit:
     ; (lines=9, args=1, vars=4, tmps=2)
     ; (after optimizer)
     ; %scollection_literal_001.php:2-7
0000 CV0($n) = RECV 1
0001 CV1($a) = CONSTRUCT_COLLECTION 0 array(...)
0002 T4 = INIT_ARRAY 2 (packed) CV0($n) NEXT
0003 T5 = ADD CV0($n) int(1)
0004 T4 = ADD_ARRAY_ELEMENT T5 NEXT
0005 CV2($b) = CONSTRUCT_COLLECTION 1 T4
0006 T4 = INIT_ARRAY 1 (packed) CV2($b) NEXT
0007 CV3($c) = CONSTRUCT_COLLECTION 2 T4
0008 RETURN CV3($c)
LIVE RANGES:
     4: 0003 - 0005 (tmp/var)
