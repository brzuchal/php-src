--TEST--
vec runtime representation: C-level self-test of construction, ownership, destruction and private-path invariants
--EXTENSIONS--
zend_test
--FILE--
<?php
$r = zend_test_vec_selftest();
ksort($r);
foreach ($r as $scenario => $ok) {
    printf("%-26s %s\n", $scenario, $ok ? "pass" : "FAIL");
}
?>
--EXPECT--
alloc_starts_empty         pass
builtin                    pass
destroys_only_installed    pass
element_dtor               pass
element_dtor_exactly_once  pass
empty                      pass
failed_append_inert        pass
named_ownership            pass
validator                  pass
