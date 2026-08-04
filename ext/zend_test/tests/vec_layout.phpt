--TEST--
vec runtime layout: header offsets/sizes, tag arithmetic, and the ABI-independent hybrid invariants (32-bit portable)
--EXTENSIONS--
zend_test
--FILE--
<?php
// Raw layout evidence + the invariants the hybrid overlay/tag depend on. The
// numbers vary by ABI (matched with %d below so they still print in the CI diff),
// but every invariant MUST hold identically on LP64, LLP64 and ILP32 -- that is
// the whole point of the portable static asserts in zend_vec.h, checked here at
// runtime on whatever target actually runs the test (notably x32).
$L = zend_test_vec_layout();

foreach (['sizeof_pointer','sizeof_zval','alignof_zval','sizeof_zend_vec',
          'offsetof_count','offsetof_capacity','offsetof_elements',
          'header_size','hybrid_alloc_size',
          'hybrid_flag_bit','cap_mask','max_capacity'] as $k) {
    printf("%-20s = %d\n", $k, $L[$k]);
}
printf("%-20s = %s\n", 'hybrid_supported', $L['hybrid_supported'] ? 'yes' : 'no');

foreach (['inv_count_before_capacity','inv_capacity_in_header',
          'inv_elements_zval_aligned','inv_overlay_is_zvals'] as $k) {
    printf("%-28s %s\n", $k, $L[$k] ? 'yes' : 'NO');
}

// Independent PHP-side cross-checks (do not trust only the C flags).
printf("capacity spans one word:  %s\n",
    ($L['offsetof_elements'] - $L['offsetof_capacity'] >= 4) ? 'yes' : 'NO');
printf("overlay fits allocation:  %s\n",
    ($L['hybrid_alloc_size'] === $L['header_size'] + 2 * $L['sizeof_zval']) ? 'yes' : 'NO');
printf("mask is 31 low bits:      %s\n",
    ($L['cap_mask'] === 2147483647) ? 'yes' : 'NO');
?>
--EXPECTF--
sizeof_pointer       = %d
sizeof_zval          = 16
alignof_zval         = %d
sizeof_zend_vec      = %d
offsetof_count       = %d
offsetof_capacity    = %d
offsetof_elements    = %d
header_size          = %d
hybrid_alloc_size    = %d
hybrid_flag_bit      = 31
cap_mask             = 2147483647
max_capacity         = 2147483647
hybrid_supported     = %s
inv_count_before_capacity    yes
inv_capacity_in_header       yes
inv_elements_zval_aligned    yes
inv_overlay_is_zvals         yes
capacity spans one word:  yes
overlay fits allocation:  yes
mask is 31 low bits:      yes
