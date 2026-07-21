--TEST--
collection type representation: recursively nested descriptors vec[vec[int]]
--EXTENSIONS--
zend_test
--FILE--
<?php
$r = zend_test_nested_collection_type_selftest();
ksort($r);
foreach ($r as $scenario => $ok) {
    printf("%-26s %s\n", $scenario, $ok ? "pass" : "FAIL");
}
?>
--EXPECT--
nested_arena_release       pass
nested_construction        pass
nested_deep_copy           pass
nested_release_ownership   pass
nested_stringify           pass
