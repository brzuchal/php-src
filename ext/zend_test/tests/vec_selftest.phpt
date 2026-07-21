--TEST--
vec runtime representation: C-level self-test of alloc, element type, ownership and destruction
--EXTENSIONS--
zend_test
--FILE--
<?php
$r = zend_test_vec_selftest();
ksort($r);
foreach ($r as $scenario => $ok) {
    printf("%-16s %s\n", $scenario, $ok ? "pass" : "FAIL");
}
?>
--EXPECT--
builtin          pass
element_dtor     pass
empty            pass
named_ownership  pass
validator        pass
