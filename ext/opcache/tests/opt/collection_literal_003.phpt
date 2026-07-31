--TEST--
Collection literal 003: set literals compile to the direct INIT/ADD/FINISH_COLLECTION builder
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
// A set literal compiles to the same builder as vec/tuple: INIT_COLLECTION allocates an
// exact-size payload, one ADD_COLLECTION_ELEMENT per *source* element (duplicates included --
// they are still evaluated), and FINISH_COLLECTION validates then deduplicates in place. No
// INIT_ARRAY / CONSTRUCT_COLLECTION and no intermediate HashTable.
function lit(int $n) {
    $a = set[int]{};
    $b = set[int]{$n, $n, $n + 1};
    $c = set[vec[int]]{vec[int]{$n}};
    return $c;
}
?>
--EXPECTF--
$_main:
     ; (lines=1, args=0, vars=0, tmps=0)
     ; (after optimizer)
     ; %scollection_literal_003.php:1-%d
0000 RETURN int(1)

lit:
     ; (lines=16, args=1, vars=4, tmps=3)
     ; (after optimizer)
     ; %scollection_literal_003.php:6-11
0000 CV0($n) = RECV 1
0001 T4 = INIT_COLLECTION 0 0
0002 CV1($a) = FINISH_COLLECTION 0 T4
0003 T4 = INIT_COLLECTION 1 3
0004 T4 = ADD_COLLECTION_ELEMENT CV0($n)
0005 T4 = ADD_COLLECTION_ELEMENT CV0($n)
0006 T5 = ADD CV0($n) int(1)
0007 T4 = ADD_COLLECTION_ELEMENT T5
0008 CV2($b) = FINISH_COLLECTION 1 T4
0009 T4 = INIT_COLLECTION 2 1
0010 T6 = INIT_COLLECTION 3 1
0011 T6 = ADD_COLLECTION_ELEMENT CV0($n)
0012 T5 = FINISH_COLLECTION 3 T6
0013 T4 = ADD_COLLECTION_ELEMENT T5
0014 CV3($c) = FINISH_COLLECTION 2 T4
0015 RETURN CV3($c)
LIVE RANGES:
     4: 0004 - 0008 (tmp/var)
     4: 0010 - 0014 (tmp/var)
     6: 0011 - 0012 (tmp/var)
