--TEST--
Collection literal 001: vec literals compile to the direct INIT/ADD/FINISH_COLLECTION builder
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
     ; (lines=12, args=1, vars=4, tmps=2)
     ; (after optimizer)
     ; %scollection_literal_001.php:2-7
0000 CV0($n) = RECV 1
0001 T4 = INIT_COLLECTION 0 0
0002 CV1($a) = FINISH_COLLECTION 0 T4
0003 T4 = INIT_COLLECTION 1 2
0004 T4 = ADD_COLLECTION_ELEMENT CV0($n)
0005 T5 = ADD CV0($n) int(1)
0006 T4 = ADD_COLLECTION_ELEMENT T5
0007 CV2($b) = FINISH_COLLECTION 1 T4
0008 T4 = INIT_COLLECTION 2 1
0009 T4 = ADD_COLLECTION_ELEMENT CV2($b)
0010 CV3($c) = FINISH_COLLECTION 2 T4
0011 RETURN CV3($c)
LIVE RANGES:
     4: 0004 - 0007 (tmp/var)
     4: 0009 - 0010 (tmp/var)
