--TEST--
collection type representation: C-level self-test of descriptor lifecycle and discriminators
--EXTENSIONS--
zend_test
--FILE--
<?php
$r = zend_test_collection_type_selftest();
ksort($r);
foreach ($r as $scenario => $ok) {
    printf("%-26s %s\n", $scenario, $ok ? "pass" : "FAIL");
}
?>
--EXPECT--
arena_release              pass
construction               pass
independent_destruction    pass
intersection_is_type_list  pass
named_ownership            pass
stringify                  pass
union_is_type_list         pass
