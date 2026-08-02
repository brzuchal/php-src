--TEST--
vec hybrid: retained append shares the base (no copy) — constant memory per branch
--EXTENSIONS--
zend_test
--FILE--
<?php
// A retained receiver ($base is a live CV) is not exclusively owned, so
// $base->append(x) SHARES the base inside a HYBRID root instead of copying it.
// Proof: the per-branch memory cost is a small constant, independent of the base
// size N -- a copy would cost ~N*16 bytes per branch.
foreach ([[0,2],[1,2],[8,8],[64,8],[1000,64],[100000,64],[100000,1000]] as [$n,$b]) {
    $vals = $n > 0 ? range(0, $n - 1) : [];
    zend_test_make_vec($vals, 'int', $base);          // FLAT base of n ints

    $m0 = memory_get_usage();
    $branches = [];
    for ($i = 0; $i < $b; $i++) {
        $branches[] = $base->append(1_000_000 + $i);  // retained -> hybrid sharing base
    }
    $per_branch = (memory_get_usage() - $m0) / $b;

    unset($branches);                                 // release every branch
    $residual = memory_get_usage() - $m0;             // base untouched -> back near 0

    printf("n=%-6d b=%-4d const_per_branch=%s no_leak=%s\n",
        $n, $b,
        $per_branch < 512 ? 'yes' : "NO($per_branch)",
        $residual < 2048 ? 'yes' : "NO($residual)");
    unset($base);
}

// Both the receiver and the branch report as collections (gettype/get_debug_type
// deliberately do not leak the descriptor), observable without reading elements.
zend_test_make_vec([1, 2], 'int', $base);
$w = $base->append(3);
printf("base type=%s  branch type=%s\n", get_debug_type($base), get_debug_type($w));
?>
--EXPECT--
n=0      b=2    const_per_branch=yes no_leak=yes
n=1      b=2    const_per_branch=yes no_leak=yes
n=8      b=8    const_per_branch=yes no_leak=yes
n=64     b=8    const_per_branch=yes no_leak=yes
n=1000   b=64   const_per_branch=yes no_leak=yes
n=100000 b=64   const_per_branch=yes no_leak=yes
n=100000 b=1000 const_per_branch=yes no_leak=yes
base type=collection  branch type=collection
