--TEST--
vec hybrid: hybrid indexed read + iteration boundaries (base region then tail)
--EXTENSIONS--
zend_test
--FILE--
<?php
// A retained append yields a hybrid whose logical order is the base elements then
// the tail element. Reads and foreach must be representation-independent.
function hy(array $base_vals, int $tail_val): mixed {
    zend_test_make_vec($base_vals, 'int', $b);   // flat base
    return $b->append($tail_val);                 // retained -> hybrid, tail=[tail_val]
}

// base=[10,20,30], tail=[40]; the base|tail boundary is at index 3.
$h = hy([10, 20, 30], 40);
echo "base[0]=$h[0] base_last=$h[2] tail_first=$h[3]\n";
$acc = []; foreach ($h as $i => $v) $acc[] = "$i:$v"; echo "foreach: ", implode(' ', $acc), "\n";

// Empty base: every read resolves into the tail (base_count == 0).
$e = hy([], 99);
echo "empty-base tail_first=$e[0] iter="; foreach ($e as $v) echo $v; echo "\n";

// Out-of-range / negative / bad-key reads: the same errors as a flat vec, no crash.
try { $x = $h[4]; }  catch (\Throwable $t) { echo "OOB: ", $t::class, "\n"; }
try { $x = $h[-1]; } catch (\Throwable $t) { echo "NEG: ", $t::class, "\n"; }
try { $x = $h['k']; } catch (\Throwable $t) { echo "BADKEY: ", $t::class, "\n"; }
echo "isset[3]=", (int)isset($h[3]), " isset[4]=", (int)isset($h[4]),
     " empty[0]=", (int)empty($h[0]), "\n";

// A larger base: every logical index maps to the right element across the boundary.
zend_test_make_vec(range(0, 99), 'int', $b100);
$h100 = $b100->append(100);        // base=[0..99], tail=[100]
$ok = true;
for ($i = 0; $i <= 100; $i++) if ($h100[$i] !== $i) { $ok = false; break; }
echo "read-all-across-boundary: ", $ok ? 'ok' : 'MISMATCH', "\n";
?>
--EXPECT--
base[0]=10 base_last=30 tail_first=40
foreach: 0:10 1:20 2:30 3:40
empty-base tail_first=99 iter=99
OOB: ValueError
NEG: ValueError
BADKEY: TypeError
isset[3]=1 isset[4]=0 empty[0]=0
read-all-across-boundary: ok
