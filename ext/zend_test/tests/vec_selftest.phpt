--TEST--
vec runtime representation: C-level self-test of construction, ownership, destruction and private-path invariants (incl. hybrid ownership)
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
hybrid_base_shared         pass
hybrid_branch_independent  pass
hybrid_dtor_balanced       pass
hybrid_tagged              pass
hybrid_tail_owned          pass
named_ownership            pass
validator                  pass
