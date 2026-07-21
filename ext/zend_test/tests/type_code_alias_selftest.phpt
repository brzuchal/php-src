--TEST--
runtime type tags above the may-be mask must not alias type flags
--EXTENSIONS--
zend_test
--FILE--
<?php
$r = zend_test_type_code_alias_selftest();
ksort($r);
foreach ($r as $scenario => $ok) {
    printf("%-30s %s\n", $scenario, $ok ? "pass" : "FAIL");
}
?>
--EXPECT--
builtins_unchanged             pass
flags_excluded                 pass
iterable_excludes_collection   pass
