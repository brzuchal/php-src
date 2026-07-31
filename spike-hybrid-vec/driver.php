<?php
/* Hybrid persistent vec spike — benchmark driver.
 *
 * Enumerates the full case matrix, runs each case in a FRESH php process
 * (GC disabled), shuffles execution order with a fixed seed, validates
 * checksums across representations/chunk sizes within logical-content
 * groups, and appends rows to results/raw.csv.
 *
 * Usage: ../sapi/cli/php -n driver.php [--only=substr] [--dry] [--out=name.csv]
 */

$PHP = getenv('PHP_BIN') ?: (__DIR__ . '/../sapi/cli/php');
$ONLY = null;
$DRY = false;
$OUT = 'raw.csv';
foreach (array_slice($argv, 1) as $a) {
    if (str_starts_with($a, '--only=')) $ONLY = substr($a, 7);
    elseif ($a === '--dry') $DRY = true;
    elseif (str_starts_with($a, '--out=')) $OUT = substr($a, 6);
}

$SHIFTS = [4, 5, 6, 7];               // chunks 16, 32, 64, 128
$ELEMS  = ['int', 'string', 'object', 'array', 'nested'];

$cases = [];
$add = function (array $spec, string $vgroup = '', array $meta = []) use (&$cases) {
    $cases[] = ['spec' => $spec, 'vgroup' => $vgroup, 'meta' => $meta];
};

/* ---- W1: linear append, intermediates dead ---- */
foreach ($ELEMS as $el) {
    foreach ([1024, 4096, 16384] as $n) {
        $s = $n >= 16384 ? ['samples' => 6, 'warmup' => 2] : [];
        $add(['workload' => 'linear', 'repr' => 'A', 'shift' => 6, 'elem' => $el,
              'n' => $n, 'retained' => 0] + $s, "lin-dead-$el-$n");
    }
    foreach ([1024, 4096, 16384, 65536] as $n) {
        $add(['workload' => 'linear', 'repr' => 'B', 'shift' => 6, 'elem' => $el,
              'n' => $n, 'retained' => 0], "lin-dead-$el-$n");
    }
    foreach ($SHIFTS as $sh) {
        foreach ([1024, 4096, 65536] as $n) {
            $s = $n >= 65536 ? ['samples' => 8, 'warmup' => 2] : [];
            $add(['workload' => 'linear', 'repr' => 'C', 'shift' => $sh, 'elem' => $el,
                  'n' => $n, 'retained' => 0] + $s, "lin-dead-$el-$n");
        }
    }
}

/* ---- W2: linear append, every version retained ---- */
foreach ($ELEMS as $el) {
    foreach ([1024, 4096] as $n) {
        $s = $n >= 4096 ? ['samples' => 6, 'warmup' => 2] : [];
        $add(['workload' => 'linear', 'repr' => 'A', 'shift' => 6, 'elem' => $el,
              'n' => $n, 'retained' => 1] + $s, "lin-ret-$el-$n");
        $add(['workload' => 'linear', 'repr' => 'B', 'shift' => 6, 'elem' => $el,
              'n' => $n, 'retained' => 1] + $s, "lin-ret-$el-$n");
    }
}
foreach (['int', 'string', 'object'] as $el) {
    foreach ($SHIFTS as $sh) {
        foreach ([1024, 4096, 16384] as $n) {
            $add(['workload' => 'linear', 'repr' => 'C', 'shift' => $sh, 'elem' => $el,
                  'n' => $n, 'retained' => 1], "lin-ret-$el-$n");
        }
        if ($el === 'int') {
            $add(['workload' => 'linear', 'repr' => 'C', 'shift' => $sh, 'elem' => $el,
                  'n' => 65536, 'retained' => 1, 'samples' => 5, 'warmup' => 1],
                 "lin-ret-$el-65536");
        }
    }
}

/* ---- W4: large flat base + append ---- */
$bases = [100, 1000, 10000, 100000];
$apps  = [1, 4, 16, 32, 64, 1024];
foreach (['int', 'string'] as $el) {
    foreach ($bases as $b) {
        foreach ($apps as $k) {
            $capAB = max(1, min(4096, intdiv(192_000_000, ($b + $k) * 16)));
            $slow = ($b >= 100000 && $k >= 1024) ? ['samples' => 6, 'warmup' => 1] : [];
            $vg = "ba-$el-$b-$k";
            $add(['workload' => 'base_append', 'repr' => 'A', 'shift' => 6, 'elem' => $el,
                  'base' => $b, 'appended' => $k, 'batch_cap' => $capAB] + $slow, $vg);
            $add(['workload' => 'base_append', 'repr' => 'B', 'shift' => 6, 'elem' => $el,
                  'base' => $b, 'appended' => $k, 'batch_cap' => $capAB] + $slow, $vg);
            foreach ($SHIFTS as $sh) {
                $add(['workload' => 'base_append', 'repr' => 'C', 'shift' => $sh, 'elem' => $el,
                      'base' => $b, 'appended' => $k, 'batch_cap' => 4096], $vg);
            }
        }
    }
}
foreach (['object'] as $el) {   /* spot-check one heavier element kind */
    foreach ([1, 64] as $k) {
        $vg = "ba-$el-10000-$k";
        $add(['workload' => 'base_append', 'repr' => 'A', 'shift' => 6, 'elem' => $el,
              'base' => 10000, 'appended' => $k, 'batch_cap' => 1024], $vg);
        $add(['workload' => 'base_append', 'repr' => 'B', 'shift' => 6, 'elem' => $el,
              'base' => 10000, 'appended' => $k, 'batch_cap' => 1024], $vg);
        $add(['workload' => 'base_append', 'repr' => 'C', 'shift' => 6, 'elem' => $el,
              'base' => 10000, 'appended' => $k, 'batch_cap' => 4096], $vg);
    }
}

/* ---- W3: branch from an older version ---- */
foreach (['int', 'string'] as $el) {
    foreach ($SHIFTS as $sh) {
        foreach (['half', 'worst', 'boundary'] as $pos) {
            $add(['workload' => 'branch', 'repr' => 'C', 'shift' => $sh, 'elem' => $el,
                  'base' => 1024, 'lv_pos' => $pos, 'batch_cap' => 4096],
                 "br-$el-$sh-$pos");
        }
    }
    foreach ([4, 7] as $sh) {
        foreach (['A', 'B'] as $r) {
            $add(['workload' => 'branch', 'repr' => $r, 'shift' => $sh, 'elem' => $el,
                  'base' => 1024, 'lv_pos' => 'half', 'batch_cap' => 1024],
                 "br-$el-$sh-half");
        }
    }
}

/* ---- W5: indexed read ---- */
foreach ($SHIFTS as $sh) {
    foreach (['base', 'full', 'last', 'mixed'] as $rg) {
        $vg = $rg === 'base' ? "rd-base" : "";
        $add(['workload' => 'read', 'repr' => 'C', 'shift' => $sh, 'elem' => 'int',
              'base' => 10000, 'blocks' => 8, 'region' => $rg,
              'batch_cap' => 2_000_000, 'target_us' => 400], $vg);
    }
}
foreach (['A', 'B'] as $r) {
    foreach (['base', 'mixed'] as $rg) {
        $add(['workload' => 'read', 'repr' => $r, 'shift' => 6, 'elem' => 'int',
              'base' => 10000, 'blocks' => 8, 'region' => $rg,
              'batch_cap' => 2_000_000, 'target_us' => 400],
             $rg === 'base' ? "rd-base" : "");
    }
}

/* ---- W6: foreach ---- */
foreach ($SHIFTS as $sh) {
    foreach ([0, 1, 8, 64, 1024] as $bl) {
        $total = 10000 + $bl * (1 << $sh);
        $add(['workload' => 'foreach', 'repr' => 'C', 'shift' => $sh, 'elem' => 'int',
              'base' => 10000, 'blocks' => $bl, 'batch_cap' => 100000,
              'target_us' => 500], "fe-int-$total");
    }
}
$totals = [10000 => true];
foreach ($SHIFTS as $sh) {
    foreach ([1, 8, 64, 1024] as $bl) $totals[10000 + $bl * (1 << $sh)] = true;
}
foreach (array_keys($totals) as $t) {
    $add(['workload' => 'foreach', 'repr' => 'A', 'shift' => 6, 'elem' => 'int',
          'base' => $t, 'blocks' => 0, 'batch_cap' => 100000, 'target_us' => 500],
         "fe-int-$t");
}
$add(['workload' => 'foreach', 'repr' => 'B', 'shift' => 6, 'elem' => 'int',
      'base' => 10000, 'blocks' => 0, 'batch_cap' => 100000, 'target_us' => 500],
     "fe-int-10000");
$add(['workload' => 'foreach', 'repr' => 'C', 'shift' => 6, 'elem' => 'string',
      'base' => 10000, 'blocks' => 8, 'batch_cap' => 100000, 'target_us' => 500],
     "fe-str-10512");
$add(['workload' => 'foreach', 'repr' => 'A', 'shift' => 6, 'elem' => 'string',
      'base' => 10512, 'blocks' => 0, 'batch_cap' => 100000, 'target_us' => 500],
     "fe-str-10512");

/* ---- W7: version creation cost vs visible block count ---- */
foreach ($SHIFTS as $sh) {
    foreach ([1, 8, 64, 1024] as $vb) {
        $add(['workload' => 'vcreate', 'repr' => 'C', 'shift' => $sh, 'elem' => 'int',
              'base' => 64, 'blocks' => $vb, 'batch_cap' => 2048], '');
    }
}

/* ---- W8: destruction ---- */
foreach ($ELEMS as $el) {
    foreach (['A', 'B'] as $r) {
        $add(['workload' => 'destroy', 'repr' => $r, 'shift' => 6, 'elem' => $el,
              'n' => 16384, 'retained' => 0, 'samples' => 8, 'warmup' => 2], '');
    }
    foreach ([4, 7] as $sh) {
        $add(['workload' => 'destroy', 'repr' => 'C', 'shift' => $sh, 'elem' => $el,
              'n' => 16384, 'retained' => 0, 'samples' => 8, 'warmup' => 2], '');
    }
}
foreach (['int', 'string'] as $el) {
    foreach (['A', 'B'] as $r) {
        $add(['workload' => 'destroy', 'repr' => $r, 'shift' => 6, 'elem' => $el,
              'n' => 2048, 'retained' => 1, 'samples' => 6, 'warmup' => 2], '');
    }
    foreach ([4, 7] as $sh) {
        $add(['workload' => 'destroy', 'repr' => 'C', 'shift' => $sh, 'elem' => $el,
              'n' => 16384, 'retained' => 1, 'samples' => 6, 'warmup' => 2], '');
    }
}

/* ---- W9: invisible retention (stats) ---- */
foreach ($SHIFTS as $sh) {
    $c = 1 << $sh;
    $add(['workload' => 'invisible', 'repr' => 'C', 'shift' => $sh, 'elem' => 'string',
          'base' => 1000, 'appended' => 40 * $c, 'n' => 10 * $c + intdiv($c, 2)], '');
    $add(['workload' => 'invisible', 'repr' => 'C', 'shift' => $sh, 'elem' => 'string',
          'base' => 1000, 'appended' => 40 * $c, 'n' => 10 * $c + 1], '');
}

/* ---- W10: forks + depth ---- */
foreach ([1, 4, 16, 64, 256] as $f) {
    $add(['workload' => 'forks', 'repr' => 'C', 'shift' => 6, 'elem' => 'int',
          'base' => 10000, 'forks' => $f, 'per' => 8, 'samples' => 8, 'warmup' => 2],
         "fk-$f");
}
foreach ([1, 16] as $f) {
    $add(['workload' => 'forks', 'repr' => 'A', 'shift' => 6, 'elem' => 'int',
          'base' => 10000, 'forks' => $f, 'per' => 8, 'samples' => 6, 'warmup' => 2],
         "fk-$f");
}
$add(['workload' => 'forks', 'repr' => 'B', 'shift' => 6, 'elem' => 'int',
      'base' => 10000, 'forks' => 16, 'per' => 8, 'samples' => 6, 'warmup' => 2],
     "fk-16");
foreach ([16, 256, 1024] as $d) {
    $add(['workload' => 'depth', 'repr' => 'C', 'shift' => 6, 'elem' => 'int',
          'base' => 1024, 'depth' => $d, 'samples' => 8, 'warmup' => 2], '');
}
$add(['workload' => 'depth', 'repr' => 'C', 'shift' => 6, 'elem' => 'int',
      'base' => 1024, 'depth' => 256, 'keep_all' => 1, 'samples' => 8, 'warmup' => 2], '');

/* ---------------- execution ---------------- */

if ($ONLY !== null) {
    $cases = array_values(array_filter($cases,
        fn($c) => str_contains(json_encode($c['spec']), $ONLY)));
}

/* deterministic shuffle of the big matrix */
mt_srand(20260731);
for ($i = count($cases) - 1; $i > 0; $i--) {
    $j = mt_rand(0, $i);
    [$cases[$i], $cases[$j]] = [$cases[$j], $cases[$i]];
}

fprintf(STDERR, "cases: %d\n", count($cases));
if ($DRY) {
    foreach ($cases as $c) echo json_encode($c['spec']), "\n";
    exit(0);
}

@mkdir(__DIR__ . '/results', 0777, true);
file_put_contents(__DIR__ . '/results/env.json', json_encode([
    'date' => date('c'),
    'uname' => trim(shell_exec('uname -a')),
    'cpu' => trim(shell_exec('sysctl -n machdep.cpu.brand_string')),
    'ncpu' => (int) trim(shell_exec('sysctl -n hw.ncpu')),
    'memsize' => (int) trim(shell_exec('sysctl -n hw.memsize')),
    'php' => trim(explode("\n", shell_exec("$PHP -n -v"))[0]),
    'git' => trim(shell_exec('git -C ' . escapeshellarg(__DIR__ . '/..') . ' rev-parse HEAD')),
    'layout' => json_decode(shell_exec("$PHP -n -r 'echo json_encode(zend_test_vec_spike_layout());'"), true),
], JSON_PRETTY_PRINT));

$cols = ['id', 'workload', 'repr', 'chunk', 'elem', 'retained', 'n', 'base',
    'appended', 'blocks', 'forks', 'depth', 'keep_all', 'region', 'lv_pos',
    'batch', 'ops_per_sample', 'nsamples', 'median_ns', 'p95_ns', 'min_ns',
    'median2_ns', 'p95_2_ns', 'allocs_op', 'bytes_op', 'zcopies_op',
    'zmoves_op', 'addref_op', 'blk_addref_op', 'branches_op', 'mem_start',
    'mem_build', 'mem_peak', 'mem_end', 'checksum', 'balance_ok', 'vgroup',
    'extra'];
$csv = fopen(__DIR__ . "/results/$OUT", 'w');
fputcsv($csv, $cols, ',', '"', '\\');

function q(array $s, float $p): float {
    sort($s);
    $i = (int) ceil($p * count($s)) - 1;
    return $s[max(0, min($i, count($s) - 1))];
}

$vg_seen = [];
$fails = 0;
$t_start = microtime(true);
foreach ($cases as $ci => $c) {
    $spec = $c['spec'];
    $json = json_encode($spec);
    $cmd = escapeshellarg($PHP) . ' -n -d zend.enable_gc=0 -d memory_limit=2G '
         . escapeshellarg(__DIR__ . '/runner.php') . ' ' . escapeshellarg($json);
    $out = shell_exec($cmd);
    $r = json_decode((string) $out, true);
    $id = $ci + 1;
    if (!is_array($r) || isset($r['error'])) {
        fprintf(STDERR, "[%d/%d] FAIL %s -> %s\n", $id, count($cases), $json,
            is_array($r) ? $r['error'] : substr((string) $out, 0, 200));
        $fails++;
        continue;
    }
    $s = $r['samples_ns'] ?? [];
    $s2 = $r['samples2_ns'] ?? [];
    $ops = (float) ($r['ops_per_sample'] ?? 1);
    $tc = $r['timed_counters'] ?? [];
    $per = fn($k) => $ops > 0 ? (($tc[$k] ?? 0) / $ops) : 0;
    $row = [
        'id' => $id,
        'workload' => $spec['workload'],
        'repr' => $spec['repr'],
        'chunk' => $spec['repr'] === 'C' ? (1 << $spec['shift']) : 0,
        'elem' => $spec['elem'],
        'retained' => $spec['retained'] ?? 0,
        'n' => $spec['n'] ?? '',
        'base' => $spec['base'] ?? '',
        'appended' => $spec['appended'] ?? '',
        'blocks' => $spec['blocks'] ?? '',
        'forks' => $spec['forks'] ?? '',
        'depth' => $spec['depth'] ?? '',
        'keep_all' => $spec['keep_all'] ?? 0,
        'region' => $spec['region'] ?? '',
        'lv_pos' => $spec['lv_pos'] ?? '',
        'batch' => $r['batch'],
        'ops_per_sample' => $ops,
        'nsamples' => count($s),
        'median_ns' => $s ? round(q($s, 0.5), 3) : '',
        'p95_ns' => $s ? round(q($s, 0.95), 3) : '',
        'min_ns' => $s ? round(min($s), 3) : '',
        'median2_ns' => $s2 ? round(q($s2, 0.5), 3) : '',
        'p95_2_ns' => $s2 ? round(q($s2, 0.95), 3) : '',
        'allocs_op' => round($per('allocs'), 4),
        'bytes_op' => round($per('bytes_alloc'), 2),
        'zcopies_op' => round($per('zval_copies'), 4),
        'zmoves_op' => round($per('zval_moves'), 4),
        'addref_op' => round($per('rc_addref'), 4),
        'blk_addref_op' => round($per('block_addref'), 4),
        'branches_op' => round($per('branches'), 6),
        'mem_start' => $r['mem']['start'],
        'mem_build' => $r['mem']['build'],
        'mem_peak' => $r['mem']['peak'],
        'mem_end' => $r['mem']['end'],
        'checksum' => $r['checksum'],
        'balance_ok' => (int) $r['balance_ok'],
        'vgroup' => $c['vgroup'],
        'extra' => isset($r['extra']) ? json_encode($r['extra']) : '',
    ];
    fputcsv($csv, array_map(fn($k) => $row[$k], $cols), ',', '"', '\\');
    fflush($csv);
    if (!$r['balance_ok']) {
        fprintf(STDERR, "[%d] BALANCE FAIL %s\n", $id, $json);
        $fails++;
    }
    $vg = $c['vgroup'];
    if ($vg !== '') {
        if (isset($vg_seen[$vg]) && $vg_seen[$vg] !== $row['checksum']) {
            fprintf(STDERR, "[%d] CHECKSUM MISMATCH group=%s %s vs %s (%s)\n",
                $id, $vg, $row['checksum'], $vg_seen[$vg], $json);
            $fails++;
        }
        $vg_seen[$vg] = $row['checksum'];
    }
    if ($id % 25 === 0 || $id === count($cases)) {
        fprintf(STDERR, "[%d/%d] %.0fs elapsed\n", $id, count($cases),
            microtime(true) - $t_start);
    }
}
fclose($csv);
fprintf(STDERR, "done: %d cases, %d failures, %.0fs\n", count($cases), $fails,
    microtime(true) - $t_start);
exit($fails ? 1 : 0);
